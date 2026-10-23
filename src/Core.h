#pragma once
#include <array>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
namespace NL {
inline constexpr unsigned textCount=224,modelCount=4;
inline constexpr std::array<unsigned,modelCount> noteArtIDs{0x97788,0x1541B,0x1541C,0x15421};
struct Random {
    std::uint64_t value=0x9E3779B97F4A7C15ull;
    std::uint64_t next() { value+=0x9E3779B97F4A7C15ull;auto z=value;z=(z^(z>>30))*0xBF58476D1CE4E5B9ull;z=(z^(z>>27))*0x94D049BB133111EBull;return z^(z>>31); }
    double unit() {return (next()>>11)*0x1.0p-53;}
    double days(double low,double high) {return low+(high-low)*unit();}
    bool chance(unsigned percent) {return next()%100<percent;}
    // Old percentage weights on 200 outcomes halve generation without rounding 0.5% away.
    bool letterChance(unsigned oldPercent) {return next()%200<oldPercent;}
};
enum Role : unsigned { Ordinary, Beggar, Bandit, Guard, Noble, Bard, Farmer, Merchant, Smith, Innkeeper, Healer, Scholar, Hunter, Worker, Settler };
inline int categoryWeight(Role role,unsigned category) {
    if (category==5 && !(role==Merchant || role==Smith || role==Innkeeper || role==Farmer || role==Worker)) return 0;
    if (category==6 && (role==Bandit || role==Hunter)) return 0;
    if ((role==Beggar || role==Bandit) && category<5) return 1;
    if (role==Guard && category==2) return 1;
    if ((category==0 && (role==Noble || role==Bard)) ||
        (category==1 && (role==Ordinary || role==Settler || role==Guard || role==Farmer)) ||
        (category==2 && (role==Noble || role==Bard || role==Settler)) ||
        (category==3 && (role==Innkeeper || role==Healer || role==Merchant)) ||
        (category==4 && (role==Healer || role==Guard || role==Worker)) ||
        (category==5 && (role==Merchant || role==Smith || role==Innkeeper)) ||
        (category==6 && (role==Settler || role==Innkeeper || role==Farmer)) ||
        (category==7 && (role==Merchant || role==Scholar || role==Hunter || role==Worker))) return 3;
    return 2;
}
// One category is selected first, then exactly one chance roll. Never eight independent rolls.
inline std::optional<unsigned> rollNPC(Random& random,Role role) {
    std::array<unsigned,8> allowed{};unsigned count=0;
    for (unsigned c=0;c<8;++c) if (categoryWeight(role,c)) allowed[count++]=c;
    const auto category=allowed[random.next()%count];
    return random.letterChance(categoryWeight(role,category)) ? std::optional<unsigned>(category) : std::nullopt;
}
// Forced debug success retains the category distribution conditional on a normal successful roll.
inline unsigned forcedNPCCategory(Random& random,Role role) {
    unsigned total=0;for (unsigned c=0;c<8;++c) total+=unsigned(categoryWeight(role,c));
    unsigned value=unsigned(random.next()%total);
    for (unsigned c=0;c<8;++c) {unsigned weight=unsigned(categoryWeight(role,c));if (value<weight) return c;value-=weight;}
    return 7;
}
inline bool useCourier(bool enabled,bool questReady,bool eventReady,bool recipientSupported) {return enabled && questReady && eventReady && recipientSupported;}
inline unsigned houseLetterCount(Random& random) {const auto roll=random.next()%200;return roll<10 ? 2 : roll<95 ? 1 : 0;}

enum class Purpose : std::uint8_t { Pocket, Bundle, ToPlayer, HateDraft, Delivered };
struct Instance { std::uint16_t text=0,uid=0; std::uint8_t model=0; std::uint32_t owner=0,sender=0,extraBase=0;std::uint16_t extraUID=0; double due=0; Purpose purpose=Purpose::Pocket;std::string body,signature; };
inline std::optional<unsigned> deliveryToken(float value) {
    if (!std::isfinite(value) || value<1 || value>=textCount*65536u || std::floor(value)!=value) return {};
    return static_cast<unsigned>(value);
}
inline std::string signatureFooter(std::string_view escapedSignature) {return escapedSignature.empty() ? std::string{} : "<br><br>"+std::string(escapedSignature);}
inline std::uint32_t key(const Instance& n) {return (std::uint32_t(n.text)<<16)|n.uid;}
inline bool rebind(Instance& n,unsigned text,unsigned oldBase,unsigned oldUID,unsigned newBase,unsigned newUID) {
    if (n.text!=text || n.extraBase!=oldBase || n.extraUID!=oldUID || newUID>65535) return false;
    n.extraBase=newBase;n.extraUID=std::uint16_t(newUID);return true;
}
inline bool readyHate(const Instance& n,double day,bool exists,bool senderAlive) {
    return n.purpose==Purpose::HateDraft && n.owner==n.sender && day>=n.due && exists && senderAlive;
}
struct ActorState { double refresh=-1;std::uint32_t pocket=0;std::array<double,4> playerDue{-1,-1,-1,-1};bool investmentSeen=false;double investmentDue=-1;std::string shop; };
struct Data {
    Random random;std::array<std::uint16_t,224> serials{};
    std::unordered_map<std::uint32_t,ActorState> actors;
    std::unordered_map<std::uint32_t,Instance> notes;
    std::uint32_t courierTarget=0,courierKey=0;bool sceneSeen=false,helperDelivery=false;
    double nextCourier=-1,bundleDue=-1;
    std::uint8_t homesSeen=0,homesProcessed=0;bool homesInitialized=false;
    std::array<std::uint32_t,7> houseSenders{};
};
inline bool eligibleToRefresh(const ActorState& s,double now) {return s.refresh<0 || now>=s.refresh;}
inline bool npcRefreshDue(const ActorState& s,double day,bool repeat) {return repeat || eligibleToRefresh(s,day);}
inline bool emptyLegacyState(const Data& d) {
    return d.actors.empty() && d.notes.empty() && std::all_of(d.serials.begin(),d.serials.end(),[](auto n) {return n==0;}) && !d.courierKey && !d.courierTarget && !d.sceneSeen && !d.helperDelivery && d.nextCourier<0 && d.bundleDue<0 && !d.homesSeen && !d.homesProcessed && !d.homesInitialized && std::all_of(d.houseSenders.begin(),d.houseSenders.end(),[](auto id) {return id==0;});
}
inline bool canStageHate(const Data& data,std::uint32_t actor) {
    return std::none_of(data.notes.begin(),data.notes.end(),[actor](const auto& row) {return row.second.sender==actor && (row.second.purpose==Purpose::HateDraft || row.second.purpose==Purpose::ToPlayer);});
}
struct Writer {
    std::vector<std::uint8_t> bytes;
    template<class T> void number(T value) {for (unsigned i=0;i<sizeof(T);++i) bytes.push_back(std::uint8_t(value>>(i*8)));}
    void real(double value) {number(std::bit_cast<std::uint64_t>(value));}
    void string(const std::string& value) {number(std::uint32_t(value.size()));bytes.insert(bytes.end(),value.begin(),value.end());}
};
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t offset=0;bool valid=true;
    template<class T> T number() {if (offset+sizeof(T)>bytes.size()) {valid=false;return 0;}T value=0;for (unsigned i=0;i<sizeof(T);++i) value|=T(bytes[offset++])<<(i*8);return value;}
    double real() {double v=std::bit_cast<double>(number<std::uint64_t>());if (!std::isfinite(v) || v< -1) valid=false;return v;}
    std::string string() {auto n=number<std::uint32_t>();if (n>16384 || offset+n>bytes.size()) {valid=false;return {};}std::string s(reinterpret_cast<const char*>(bytes.data()+offset),n);offset+=n;return s;}
};
inline std::vector<std::uint8_t> encode(const Data& data) {
    Writer w;w.number(data.random.value);for (auto s:data.serials) w.number(s);
    w.number(std::uint32_t(data.actors.size()));
    for (const auto& [id,a]:data.actors) {w.number(id);w.real(a.refresh);w.number(a.pocket);for (auto v:a.playerDue) w.real(v);w.number(std::uint8_t(a.investmentSeen));w.real(a.investmentDue);w.string(a.shop);}
    w.number(std::uint32_t(data.notes.size()));
    for (const auto& [id,n]:data.notes) {w.number(id);w.number(n.text);w.number(n.model);w.number(n.uid);w.number(n.owner);w.number(n.sender);w.number(n.extraBase);w.number(n.extraUID);w.real(n.due);w.number(std::uint8_t(n.purpose));w.string(n.body);w.string(n.signature);}
    w.number(data.courierTarget);w.number(data.courierKey);w.number(std::uint8_t(data.sceneSeen));w.real(data.nextCourier);w.real(data.bundleDue);
    w.number(data.homesSeen);w.number(data.homesProcessed);w.number(std::uint8_t(data.homesInitialized));for (auto a:data.houseSenders) w.number(a);w.number(std::uint8_t(data.helperDelivery));
    return w.bytes;
}
inline std::optional<Data> decode(std::span<const std::uint8_t> bytes,unsigned schema=2) {
    if (schema!=1 && schema!=2) return {};
    Reader r{bytes};Data d;d.random.value=r.number<std::uint64_t>();for (auto& s:d.serials) s=r.number<std::uint16_t>();
    auto actors=r.number<std::uint32_t>();if (actors>100000) return {};
    for (unsigned i=0;i<actors && r.valid;++i) {auto id=r.number<std::uint32_t>();ActorState a;a.refresh=r.real();a.pocket=r.number<std::uint32_t>();for (auto& v:a.playerDue) v=r.real();auto seen=r.number<std::uint8_t>();a.investmentSeen=seen!=0;a.investmentDue=r.real();a.shop=r.string();if (!id || seen>1 || !d.actors.emplace(id,a).second) return {};}
    auto count=r.number<std::uint32_t>();if (count>1000000) return {};
    for (unsigned i=0;i<count && r.valid;++i) {auto id=r.number<std::uint32_t>();Instance n;n.text=r.number<std::uint16_t>();if (schema>=2) n.model=r.number<std::uint8_t>();n.uid=r.number<std::uint16_t>();n.owner=r.number<std::uint32_t>();n.sender=r.number<std::uint32_t>();n.extraBase=r.number<std::uint32_t>();n.extraUID=r.number<std::uint16_t>();n.due=r.real();auto p=r.number<std::uint8_t>();n.purpose=Purpose(p);n.body=r.string();n.signature=r.string();if (n.text>=224 || n.model>=modelCount || !n.uid || p>4 || key(n)!=id || n.uid>d.serials[n.text] || !d.notes.emplace(id,n).second) return {};}
    d.courierTarget=r.number<std::uint32_t>();d.courierKey=r.number<std::uint32_t>();auto scene=r.number<std::uint8_t>();d.sceneSeen=scene!=0;d.nextCourier=r.real();d.bundleDue=r.real();
    d.homesSeen=r.number<std::uint8_t>();d.homesProcessed=r.number<std::uint8_t>();auto initialized=r.number<std::uint8_t>();d.homesInitialized=initialized!=0;for (auto& a:d.houseSenders) a=r.number<std::uint32_t>();
    if (schema>=2) {auto helper=r.number<std::uint8_t>();if (helper>1) return {};d.helperDelivery=helper!=0;}
    if (!r.valid || r.offset!=bytes.size() || scene>1 || initialized>1 || d.homesSeen>127 || d.homesProcessed>127) return {};
    for (const auto& [id,a]:d.actors) if (a.pocket && !d.notes.contains(a.pocket)) return {};
    if (d.courierKey && !d.notes.contains(d.courierKey)) return {};
    if (d.helperDelivery && (!d.courierKey || !d.courierTarget)) return {};
    return d;
}
}
