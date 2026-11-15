"""Validate the authored catalog and render its complete proofreading copy."""
import json
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NPC = [
    ('love', 'Love and Romance Notes'),
    ('sympathy', 'Sympathy and Condolence Notes'),
    ('congratulations', 'Congratulations and Milestone Notes'),
    ('thanks', 'Thank-You Notes'),
    ('encouragement', 'Get-Well and Encouragement Notes'),
    ('business', 'Business and Client Notes'),
    ('welcome', 'Welcome Notes'),
    ('personal', 'Personal Lists and Notes'),
]
PLAYER = [
    ('player_lover', 'Lover Notes'),
    ('player_jealousy', 'Jealousy Notes'),
    ('player_investment', 'Investment Notes'),
    ('player_thanks', 'Thank-You Notes'),
    ('player_hate', 'I Hate You Notes'),
    ('welcome_breezehome', 'House Welcome: Breezehome'),
    ('welcome_honeyside', 'House Welcome: Honeyside'),
    ('welcome_vlindrel', 'House Welcome: Vlindrel Hall'),
    ('welcome_hjerim', 'House Welcome: Hjerim'),
    ('welcome_proudspire', 'House Welcome: Proudspire Manor'),
    ('welcome_severin', 'House Welcome: Severin Manor'),
    ('welcome_shadowfoot', 'House Welcome: Shadowfoot Sanctum'),
]
SIGNATURES = {
    **{k: '{random_signature}' for k, _ in NPC},
    'personal': '',
    'player_lover': '{spouse_first_name}',
    'player_jealousy': '{other_lover_initial}',
    'player_investment': '{merchant_name}, {shop}',
    'player_thanks': '{friend_name_or_initial}',
    'player_hate': '{rival_name_or_initial}',
    **{k: '{local_resident_name_or_initial}' for k, _ in PLAYER if k.startswith('welcome_')},
}

def main():
    notes = json.loads((ROOT / 'content/notes.json').read_text(encoding='utf-8'))
    expected = {k: 20 for k, _ in NPC}
    expected.update(dict(player_lover=10, player_jealousy=5, player_investment=5, player_thanks=15, player_hate=15))
    expected.update({k: 2 for k, _ in PLAYER if k.startswith('welcome_')})
    assert Counter(n['category'] for n in notes) == Counter(expected), 'Incorrect category counts'
    for key in ('id', 'body'):
        values = [n[key].strip().casefold() for n in notes]
        assert len(set(values)) == len(notes) and all(values), f'Duplicate or empty {key}'
    assert all(n['title'] in {'Note','Letter','Personal Note','Shopping List'} for n in notes)
    assert len(notes) == 224
    for note in notes:
        assert note['signature'] == SIGNATURES[note['category']]
        assert note['audience'] == ('npc' if note['category'] in dict(NPC) else 'player')
        assert '{' not in note['title'] and '\n' not in note['title']
        assert ('{shop}' in note['body']) == (note['category'] == 'player_investment')
        if note['category'] == 'personal':
            assert not note['signature']
        if note['category'] == 'player_hate':
            assert note['body'].startswith('{player_name},\n\n')
        if note.get('kind') == 'list':
            vanilla = {'Salt Pile', 'Bread', 'Red Apple', 'Honey', 'Lockpick', 'Torch', 'Leather Strips',
                       'Lavender', 'Blue Mountain Flower', 'Thistle Branch', 'Juniper Berries', 'Linen Wrap',
                       'Leather', 'Goat Cheese Wedge', 'Elves Ear', 'Nord Mead', 'Iron Ingot', 'Steel Ingot',
                       'Corundum Ingot', 'Sweet Roll', 'Honey Nut Treat', 'Boiled Creme Treat', 'Alto Wine',
                       'Frost Mirriam', 'Garlic', 'Snowberries', 'Potion of Minor Healing', 'Wooden Plate',
                       'Wooden Bowl', 'Tankard'}
            assert all(item in vanilla for item in note['body'].splitlines()), 'Non-vanilla list entry'
    lines = [
        '# Notes and Letters — Complete Proofreading Draft', '',
        '224 notes: 160 NPC notes and 64 player letters. Draft 1.', '',
        'Each heading contains a stable review ID and the broad in-game note title. '
        'Use the ID when requesting edits. Text beneath the heading is the complete note body.', '',
        'Braced text is a runtime placeholder, not literal in-game text. NPC signatures are '
        'chosen per note instance; repeats are acceptable. Personal notes and lists have no signature. Player letters use the appropriate '
        'sender. House welcomes are signed by a resident of the relevant settlement.', '',
        'NPC notes are self-contained flavor: no quests, rewards, appointments, requests, '
        'or specific off-page actions. Personal lists are fragments rather than instructions. '
        'Player letters refer only to their stated relationship, investment, or home ownership context.', '',
        '## Contents', '',
        '- NPC notes: eight categories, 20 each.',
        '- Player letters: lover 10, jealousy 5, investment 5, friendship 15, hatred 15, house welcomes 14.', '',
    ]
    for audience, groups in [('NPC Notes — 160', NPC), ('Player Letters — 64', PLAYER)]:
        lines.extend([f'## {audience}', ''])
        for category, heading in groups:
            selected = [n for n in notes if n['category'] == category]
            lines.extend([f'### {heading} ({len(selected)})', ''])
            for n in selected:
                lines.extend([f"#### {n['id']} — {n['title']}", '', n['body'].replace('\n', '  \n'), ''])
                if n['signature']:
                    lines.extend([f"— {n['signature']}", ''])
    target = ROOT / 'Notes-and-Letters-Proofreading.md'
    target.write_text('\n'.join(lines), encoding='utf-8')
    print(f'Validated {len(notes)} notes with unique bodies and IDs; 160 NPC + 64 player notes. Wrote {target.name}.')

if __name__ == '__main__':
    main()
