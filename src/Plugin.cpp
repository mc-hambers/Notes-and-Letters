#include "Core.h"
#include "Catalog.h"
#include "DebugConfig.h"
#include "RuntimeExtraData.h"
#include "FormResolution.h"
#include <fstream>
#include <filesystem>
#include <new>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <set>
namespace logger=SKSE::log;
namespace {
using namespace NL;
using namespace std::chrono_literals;
constexpr unsigned saveID=0x4E4C3031,recordID=0x4E4F5445;
constexpr std::array<unsigned,22> humanRaces{0x13740,0x13741,0x13742,0x13743,0x13744,0x13745,0x13746,0x13747,0x13748,0x13749,0x8883A,0x8883C,0x8883D,0x88840,0x88844,0x88845,0x88794,0xA82B9,0x88846,0x88884,0x67CD8,0xA82BA};
std::string replace(std::string s,std::string_view from,std::string_view to) {std::size_t p=0;while ((p=s.find(from,p))!=std::string::npos) {s.replace(p,from.size(),to);p+=to.size();}return s;}
std::string escaped(std::string s) {s=replace(s,"&","&amp;");s=replace(s,"<","&lt;");s=replace(s,">","&gt;");return replace(s,"\n","<br>");}
std::string first(std::string s) {auto p=s.find(' ');return s.substr(0,p);}
std::string initial(std::string s) {if (s.empty()) return "?";auto c=static_cast<unsigned char>(s[0]);unsigned count=c<0x80 ? 1 : c<0xE0 ? 2 : c<0xF0 ? 3 : 4;return s.substr(0,count)+".";}
struct Movement {unsigned book,uid,oldOwner,newOwner;};
struct UniqueChange {unsigned book,oldBase,newBase,oldUID,newUID;};
struct DeliveryNotice {unsigned note;bool ready;};
class Manager final : public RE::BSTEventSink<RE::TESObjectLoadedEvent>,public RE::BSTEventSink<RE::TESContainerChangedEvent>,public RE::BSTEventSink<RE::TESTrackedStatsEvent>,public RE::BSTEventSink<RE::TESUniqueIDChangeEvent>,public RE::BSTEventSink<SKSE::CrosshairRefEvent>,public RE::BSTEventSink<SKSE::ModCallbackEvent> {
public:
    static Manager& get() {static Manager m;return m;}
    Config config;RE::ObjectRefHandle debugTarget;bool repeatNotes=false,eventBridgeReady=false;
    Data data;std::atomic_bool active{false};std::atomic_uint64_t session{0};bool valid=true;
    std::array<RE::TESObjectBOOK*,textCount*modelCount> books{};
    std::vector<unsigned> remoteActors;std::size_t remoteCursor=0;std::chrono::steady_clock::time_point discoveredAt{};
    RE::TESQuest *npcQuest=nullptr,*playerQuest=nullptr,*marriage=nullptr;
    RE::TESObjectREFR *courier=nullptr,*container=nullptr;RE::TESGlobal* itemCount=nullptr;
    RE::BGSKeyword* habitation=nullptr;
    std::array<RE::TESObjectCELL*,7> homes{};
    void initialize() {
        std::ifstream ini(std::filesystem::path(REL::Module::get().filePath().data()).parent_path()/"Data/SKSE/Plugins/NotesAndLetters.ini");
        if (ini) config=readConfig(ini);setDebug(config.debug);
        auto* h=RE::TESDataHandler::GetSingleton();
        unsigned repairedArt=0;
        for (unsigned i=0;i<books.size();++i) {
            books[i]=h->LookupForm<RE::TESObjectBOOK>(0x800+i,"NotesAndLetters.esp");
            auto* art=h->LookupForm<RE::TESObjectSTAT>(noteArtIDs[i/textCount],"Skyrim.esm");
            if (books[i] && !books[i]->inventoryModel && art) {books[i]->inventoryModel=art;++repairedArt;}
        }
        logger::info("Paper-note model variants={}; repaired {} missing art links",modelCount,repairedArt);
        npcQuest=h->LookupForm<RE::TESQuest>(0x800,"WICourierRadiant.esp");
        playerQuest=h->LookupForm<RE::TESQuest>(0x39F82,"Skyrim.esm");marriage=h->LookupForm<RE::TESQuest>(0x21382,"Skyrim.esm");
        courier=resolveReference<RE::TESObjectREFR>(h,0x39FB7);container=resolveReference<RE::TESObjectREFR>(h,0x39FB9);itemCount=h->LookupForm<RE::TESGlobal>(0x39FBB,"Skyrim.esm");
        habitation=h->LookupForm<RE::BGSKeyword>(0x39793,"Skyrim.esm");
        constexpr std::array<unsigned,5> ids{0x165A8,0x16BDD,0x16DFA,0x16778,0x16A06};
        for (unsigned i=0;i<5;++i) homes[i]=h->LookupForm<RE::TESObjectCELL>(ids[i],"Skyrim.esm");
        homes[5]=h->LookupForm<RE::TESObjectCELL>(0x17EBE,"Dragonborn.esm");
        // Optional Creation Club home: locate its actual CELL rather than guessing a local FormID.
        for (auto* cell:h->GetFormArray<RE::TESObjectCELL>()) if (cell && std::string_view(cell->GetName())=="Shadowfoot Sanctum") homes[6]=cell;
        const auto resolved=std::count_if(books.begin(),books.end(),[](auto* b) {return b!=nullptr;});
        formsReady=std::size_t(resolved)==books.size() && playerQuest && courier && container && itemCount && habitation;
        logger::info("Required forms: books={}/896, player quest={}, courier ACHR={}, container={}, item count={}, habitation={}",resolved,playerQuest!=nullptr,courier!=nullptr,container!=nullptr,itemCount!=nullptr,habitation!=nullptr);
        if (!formsReady) logger::error("Missing required records; enable NotesAndLetters.esp and check the required-form diagnostic above");
        if (auto* crosshair=SKSE::GetCrosshairRefEventSource()) crosshair->AddEventSink(this);
        if (auto* events=SKSE::GetModCallbackEventSource()) events->AddEventSink(this);
        auto* src=RE::ScriptEventSourceHolder::GetSingleton();src->AddEventSink<RE::TESObjectLoadedEvent>(this);src->AddEventSink<RE::TESContainerChangedEvent>(this);src->AddEventSink<RE::TESTrackedStatsEvent>(this);src->AddEventSink<RE::TESUniqueIDChangeEvent>(this);
        logger::info("Catalog resolved={}; NPC courier integration={}; optional Shadowfoot cell={}",formsReady,eventBridgeReady,homes[6]!=nullptr);
        logger::info("NPC courier requested={}; available={}; direct-placement fallback active={}",config.useNPCCourier,eventBridgeReady,!useCourier(config.useNPCCourier,npcQuest!=nullptr,eventBridgeReady,true));
        worker=std::jthread([this](std::stop_token stop) {unsigned tick=0;while (!stop.stop_requested()) {std::this_thread::sleep_for(250ms);if (active && !stop.stop_requested()) {if (++tick%10==0) queueScan();}}});
    }
    void reset() {remoteActors.clear();remoteCursor=0;discoveredAt={};repeatNotes=false;debugTarget={};active=false;++session;valid=true;data={};data.random.value=std::chrono::high_resolution_clock::now().time_since_epoch().count();queued=false;{std::scoped_lock lock(inboxMutex);movements.clear();uniqueChanges.clear();investments.clear();deliveryNotices.clear();}}
    std::string inactiveReason() const {
        if (!formsReady) return "Notes and Letters: required game/plugin records are unresolved. Enable NotesAndLetters.esp; see the Required forms line in NotesAndLetters.log.";
        if (!valid) return "Notes and Letters: saved note metadata is unreadable. Load an earlier matching ESS/SKSE save; existing notes have not been regenerated.";
        return "Notes and Letters: no active loaded game. Load a save and wait for startup.";
    }
    void refreshEventBridge() {
        bool available=false;
        if (config.useNPCCourier && npcQuest) {
            auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();RE::BSTSmartPointer<RE::BSScript::Object> script;
            auto handle=vm->GetObjectHandlePolicy()->GetHandleForObject(npcQuest->GetFormType(),npcQuest);
            available=vm->FindBoundObject(handle,"NL_NPCNoteDelivery",script) && script;
        }
        if (available!=eventBridgeReady) logger::info("Optional courier delivery event available={}",available);
        eventBridgeReady=available;
    }
    void enter() {
        if (!formsReady || !valid) {active=false;report(inactiveReason());return;}
        // One-time migration only: stop the retired 0.1.5 quest, without operating its AI or aliases.
        if (auto* legacy=RE::TESDataHandler::GetSingleton()->LookupForm<RE::TESQuest>(0xC00,"NotesAndLetters.esp");legacy && legacy->IsRunning()) {legacy->Stop();logger::info("Stopped retired courier helper quest");}
        data.helperDelivery=false;data.courierKey=0;data.courierTarget=0;data.sceneSeen=false;
        queued=false;notificationOutstanding=false;active=true;refreshEventBridge();queueScan();
        logger::info("Notes enabled: {} NPC states, {} note instances; courier event={}",data.actors.size(),data.notes.size(),eventBridgeReady);
    }
    template<class F> void queue(F f) {auto epoch=session.load();SKSE::GetTaskInterface()->AddTask([this,epoch,f=std::move(f)] {if (active && session==epoch) f();});}
    void queueScan() {if (queued.exchange(true)) return;queue([this] {queued=false;scan();});}
    double now() const {return RE::Calendar::GetSingleton()->GetDaysPassed();}
    bool eligible(RE::Actor* a) const {
        if (!a || a->IsPlayerRef() || a==courier || !a->GetActorBase() || !a->GetRace() || a->IsChild() || a->IsDead() || a->IsDeleted() || a->IsDisabled() || a->IsSummoned() || a->GetCommandingActor()) return false;
        const auto race=a->GetRace()->GetFormID();return std::find(humanRaces.begin(),humanRaces.end(),race)!=humanRaces.end();
    }
    bool faction(RE::Actor* a,unsigned id) const {auto* f=RE::TESForm::LookupByID<RE::TESFaction>(id);return f && a->IsInFaction(f);}
    Role role(RE::Actor* a) const {
        if (faction(a,0x1BCC0) || faction(a,0x43599)) return Bandit;
        if (a->IsGuard()) return Guard;
        if (faction(a,0x60028)) return Beggar;
        if (faction(a,0x50920) || faction(a,0x50922) || faction(a,0x102D60)) return Noble;
        if (faction(a,0x53514)) return Bard;
        if (faction(a,0x5091B)) return Innkeeper;
        if (faction(a,0x5091D)) return Smith;
        if (faction(a,0x51596)) return Merchant;
        if (faction(a,0x51591)) return Farmer;
        if (faction(a,0x4E509) || faction(a,0xAC9C2)) return Hunter;
        if (faction(a,0x5091C) || faction(a,0x5159A)) return Healer;
        if (faction(a,0x51597) || faction(a,0x51595)) return Worker;
        if (faction(a,0x1F259)) return Scholar;
        return Ordinary;
    }
    RE::BGSLocation* settlement(RE::BGSLocation* loc) const {
        std::set<RE::BGSLocation*> seen;RE::BGSLocation* result=nullptr;
        while (loc && seen.size()<64 && seen.insert(loc).second) {if (habitation && loc->HasKeyword(habitation)) result=loc;loc=loc->parentLoc;}return result;
    }
    bool courierEligible(RE::Actor* a) const {return useCourier(config.useNPCCourier,npcQuest!=nullptr,eventBridgeReady,eligible(a) && a->GetActorBase()->IsUnique() && settlement(a->GetCurrentLocation()) && role(a)!=Bandit);}
    bool visibleRecipient(RE::Actor* actor) const {
        auto* player=RE::PlayerCharacter::GetSingleton();if (!actor || !player || !actor->Is3DLoaded()) return false;
        auto* cell=actor->GetParentCell();auto* playerCell=player->GetParentCell();
        return cell && playerCell && (cell==playerCell || (!cell->IsInteriorCell() && !playerCell->IsInteriorCell() && actor->GetWorldspace()==player->GetWorldspace()));
    }
    bool routeCourier(RE::Actor* actor) const {return courierEligible(actor) && visibleRecipient(actor) && settlement(actor->GetCurrentLocation())==settlement(RE::PlayerCharacter::GetSingleton()->GetCurrentLocation());}
    RE::TESObjectBOOK* bookFor(const Instance& note) const {return note.model<modelCount && note.text<textCount ? books[note.model*textCount+note.text] : nullptr;}
    unsigned pick(unsigned category) {std::vector<unsigned> choices;for (unsigned i=0;i<224;++i) if (catalog[i].category==category) choices.push_back(i);return choices[data.random.next()%choices.size()];}
    std::string signature() {constexpr std::string_view letters="ABDEGHJKLMNRSTV";std::string s;s+=letters[data.random.next()%letters.size()];s+='.';if (data.random.chance(40)) {s+=' ';s+=letters[data.random.next()%letters.size()];s+='.';}return s;}
    std::optional<std::uint32_t> create(unsigned index,RE::TESObjectREFR* owner,Purpose purpose,RE::Actor* sender=nullptr,std::string sign={},std::string shop={}) {
        if (index>=224 || !books[index] || !owner || data.notes.size()>=1000000 || data.serials[index]==65535) {logger::error("Note creation stopped: missing owner/form or persistence limit");return {};}
        Instance n;n.text=std::uint16_t(index);n.model=std::uint8_t(data.random.next()%modelCount);auto* book=bookFor(n);if (!book) return {};n.uid=++data.serials[index];n.extraBase=book->GetFormID();n.extraUID=n.uid;n.owner=owner->GetFormID();n.sender=sender ? sender->GetFormID() : 0;n.purpose=purpose;
        n.body=replace(std::string(catalog[index].body),"{player_name}",RE::PlayerCharacter::GetSingleton()->GetName());n.body=replace(n.body,"{shop}",shop.empty() ? "your shop" : shop);
        n.signature=catalog[index].signature.empty() ? "" : sign.empty() ? signature() : sign;
        auto* extra=allocateRuntimeExtraData<RE::ExtraDataList>([](std::size_t bytes) {return RE::malloc(bytes);},[](void* storage) {::new(storage) RE::ExtraDataList();});
        if (!extra) {logger::error("Note item-data allocation failed");return {};}
        auto id=key(n);data.notes.emplace(id,n);
        extra->Add(new RE::ExtraUniqueID(book->GetFormID(),n.uid));
        owner->AddObjectToContainer(book,extra,1,nullptr);
        logger::info("Added {} instance {} to {:08X}",catalog[index].id,n.uid,n.owner);return id;
    }
    RE::ExtraDataList* find(RE::TESObjectREFR* owner,const Instance& n) const {
        auto* book=bookFor(n);if (!owner || !book) return nullptr;
        auto inv=owner->GetInventory([&](RE::TESBoundObject& o) {return &o==book;});
        auto it=inv.find(book);if (it==inv.end() || it->second.first<=0 || !it->second.second || !it->second.second->extraLists) return nullptr;
        for (auto* x:*it->second.second->extraLists) if (x) {auto* u=x->GetByType<RE::ExtraUniqueID>();if (u && u->baseID==n.extraBase && u->uniqueID==n.extraUID) return x;}return nullptr;
    }
    bool transfer(std::uint32_t id,RE::TESObjectREFR* from,RE::TESObjectREFR* to) {
        auto it=data.notes.find(id);if (it==data.notes.end()) return false;auto* extra=find(from,it->second);if (!extra) return false;
        from->RemoveItem(bookFor(it->second),1,RE::ITEM_REMOVE_REASON::kRemove,extra,to);it->second.owner=to ? to->GetFormID() : 0;return true;
    }
    bool sendPlayer(std::uint32_t id,RE::TESObjectREFR* from) {
        if (!container || !itemCount || !transfer(id,from,container)) return false;
        data.notes.at(id).purpose=Purpose::ToPlayer;
        notifyCourier();
        logger::info("Queued player letter {:08X} in vanilla courier container",id);return true;
    }
    void notifyCourier() {
        if (notificationOutstanding && itemCount->value<=0) return;
        auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
        const auto handle=vm->GetObjectHandlePolicy()->GetHandleForObject(playerQuest->GetFormType(),playerQuest);
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
        if (!vm->DispatchMethodCall(handle,"WICourierScript","incrementItemCount",RE::MakeFunctionArguments(),callback)) {
            logger::error("Vanilla courier notification failed; letter remains safely in the courier container");
        } else notificationOutstanding=true;
    }
    void directPlayerLetter(unsigned category,RE::Actor* sender,std::string sign,std::string shop={}) {
        if (auto id=create(pick(category),container,Purpose::ToPlayer,sender,std::move(sign),std::move(shop))) {notifyCourier();logger::info("Player letter queued {:08X}",*id);}
    }
    int relationship(RE::Actor* a) const {
        RE::TESConditionItem item;item.data.functionData.function=RE::FUNCTION_DATA::FunctionID::kGetRelationshipRank;
        item.data.functionData.params[0]=RE::PlayerCharacter::GetSingleton();item.data.flags.opCode=RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
        RE::ConditionCheckParams params(a,RE::PlayerCharacter::GetSingleton());
        for (int rank=-4;rank<=4;++rank) {item.data.comparisonValue.f=static_cast<float>(rank);if (item.IsTrue(params)) return rank;}return 0;
    }
    RE::Actor* spouse() const {if (!marriage || !marriage->IsRunning()) return nullptr;auto ref=marriage->GetAliasedRef(0).get();return ref ? ref->As<RE::Actor>() : nullptr;}
    void playerLetters(RE::Actor* a,ActorState& s,double day) {
        if (!a->GetActorBase()->IsUnique()) return;
        auto* partner=spouse();const auto rank=relationship(a);
        const std::array<bool,4> allowed{a==partner,partner && a!=partner && rank==4,rank>=1 && a!=partner,rank==-1 || rank==-2};
        constexpr std::array<double,4> low{3,14,5,7},high{5,21,7,14};constexpr std::array<unsigned,4> odds{5,1,2,1};
        constexpr std::array<unsigned,4> categories{8,9,11,12};
        for (unsigned i=0;i<4;++i) {
            if (!allowed[i]) continue;if (s.playerDue[i]<0) {s.playerDue[i]=day+data.random.days(low[i],high[i]);continue;}if (day<s.playerDue[i]) continue;
            s.playerDue[i]=day+data.random.days(low[i],high[i]);if (!data.random.chance(odds[i])) continue;
            if (i==3) {if (!canStageHate(data,a->GetFormID())) continue;if (auto id=create(pick(categories[i]),a,Purpose::HateDraft,a,a->GetName())) data.notes.at(*id).due=day+data.random.days(2,3);}
            else {std::string sign=i==0 ? first(a->GetName()) : i==1 ? initial(a->GetName()) : data.random.chance(50) ? a->GetName() : initial(a->GetName());directPlayerLetter(categories[i],a,std::move(sign));}
        }
    }
    void visit(RE::Actor* a,bool relationships=true) {
        if (!eligible(a)) return;const auto id=a->GetFormID();if (data.actors.size()>=100000 && !data.actors.contains(id)) return;
        auto& s=data.actors[id];double day=now();
        logger::debug("NPC check {:08X}: role={}, refresh={}, day={}, outstanding={}",id,unsigned(role(a)),s.refresh,day,s.pocket);
        if (npcRefreshDue(s,day,repeatNotes)) {
            s.refresh=day+data.random.days(3,5);
            if (s.pocket) {
                auto it=data.notes.find(s.pocket);
                auto* owner=it==data.notes.end() ? nullptr : it->second.owner==id ? static_cast<RE::TESObjectREFR*>(a) : it->second.owner==courier->GetFormID() ? courier : nullptr;
                if (!owner || !find(owner,it->second)) s.pocket=0;
                else if (owner==a && (repeatNotes || data.random.chance(50))) {if (transfer(s.pocket,owner,nullptr)) s.pocket=0;}
            }
            if (!s.pocket) if (auto category=repeatNotes ? std::optional<unsigned>(forcedNPCCategory(data.random,role(a))) : rollNPC(data.random,role(a))) {
                logger::debug("NPC roll succeeded {:08X}: category={}, courier={}",id,*category,routeCourier(a));
                // A supported recipient waits for a real courier handoff; unsupported recipients get it directly.
                auto* owner=routeCourier(a) ? courier : static_cast<RE::TESObjectREFR*>(a);
                if (auto note=create(pick(*category),owner,Purpose::Pocket)) {s.pocket=*note;data.notes.at(*note).sender=id;}
            }
        }
        if (s.pocket && !routeCourier(a)) {
            auto pending=data.notes.find(s.pocket);
            if (pending!=data.notes.end() && pending->second.owner==courier->GetFormID()) {
                if (transfer(s.pocket,courier,a)) {s.refresh=day+data.random.days(3,5);logger::info("Direct fallback delivered pending note to {:08X}",id);}
                if (data.courierKey==s.pocket) {data.courierKey=0;data.courierTarget=0;data.sceneSeen=false;}
            }
        }
        if (relationships) playerLetters(a,s,day);
    }
    static void report(const std::string& text) {logger::info("{}",text);if (auto* console=RE::ConsoleLog::GetSingleton()) console->Print("%s",text.c_str());}
    void setDebug(bool enabled) {config.debug=enabled;if (!enabled && repeatNotes) {repeatNotes=false;data.nextCourier=now()+0.5;}spdlog::set_level(enabled ? spdlog::level::debug : spdlog::level::info);spdlog::flush_on(enabled ? spdlog::level::debug : spdlog::level::info);report(enabled ? "Notes and Letters: debug mode ON" : "Notes and Letters: debug mode OFF");}
    void toggleRepeat() {
        if (!config.debug) {report("Notes and Letters: enable debug mode with NLDebug first.");return;}
        if (!active || !valid || !formsReady) {report(inactiveReason());return;}
        repeatNotes=!repeatNotes;
        if (!repeatNotes) {data.nextCourier=repeatNotes ? now() : now()+0.5;report("Notes and Letters: repeat mode OFF. Normal NPC chances, refresh delays, and courier cooldown resume.");return;}
        report("Notes and Letters: repeat mode ON. NPC refresh waits are bypassed, note rolls always succeed, and courier cooldown is zero; one pending note per NPC.");
        if (!useCourier(config.useNPCCourier,npcQuest!=nullptr,eventBridgeReady,true)) report("Notes and Letters: NPC courier integration is unavailable/disabled; repeat notes use direct inventory placement.");
        else report("Notes and Letters: close the console; eligible requests now use the optional courier delivery event.");
        queueScan();
    }
    void debugSpawn(bool deliver=false) {
        if (!config.debug) {report("Notes and Letters: run NLDebug to enable debug mode first.");return;}
        if (!active || !valid) {report(inactiveReason());return;}
        flush();auto target=debugTarget.get();
        if (!target) if (auto* crosshair=RE::CrosshairPickData::GetSingleton()) target=crosshair->GetActiveTarget().get();
        auto* actor=target ? target->As<RE::Actor>() : nullptr;
        if (!eligible(actor) || !actor->Is3DLoaded()) {report("Notes and Letters: aim at an eligible living adult NPC, then open the console.");return;}
        const auto actorID=actor->GetFormID();if (data.actors.size()>=100000 && !data.actors.contains(actorID)) {report("Notes and Letters: actor persistence limit reached.");return;}
        auto& state=data.actors[actorID];
        if (state.pocket) {
            auto n=data.notes.find(state.pocket);auto* owner=n==data.notes.end() ? nullptr : RE::TESForm::LookupByID<RE::TESObjectREFR>(n->second.owner);
            if (n==data.notes.end() || !owner || !find(owner,n->second)) state.pocket=0;
            else if (owner!=actor && owner!=courier) state.pocket=0;
        }
        if (!debugSpawnAllowed(config.debug,active,eligible(actor),state.pocket!=0)) {report("Notes and Letters: this NPC already has an outstanding pocket note (including queued deliveries).");return;}
        const auto occupation=role(actor);const auto category=forcedNPCCategory(data.random,occupation);const auto index=pick(category);
        auto* owner=deliver && routeCourier(actor) ? courier : static_cast<RE::TESObjectREFR*>(actor);
        if (auto id=create(index,owner,Purpose::Pocket)) {
            state.pocket=*id;data.notes.at(*id).sender=actorID;state.refresh=now()+data.random.days(3,5);
            if (owner==courier) {report("Notes and Letters: delivery event queued; close the console so the courier can approach.");queue([this] {deliveries();});}
            else if (deliver) report("Notes and Letters: event patch unavailable or recipient unsupported; placed directly in inventory.");
            report(fmt::format("Notes and Letters: '{}' created to {} ({:08X}); category {}, role {}, refresh day {:.3f}.",catalog[index].title,actor->GetName(),actorID,category,unsigned(occupation),state.refresh));
        } else report("Notes and Letters: note creation failed; see NotesAndLetters.log.");
    }
    RE::BSEventNotifyControl ProcessEvent(const SKSE::CrosshairRefEvent* event,RE::BSTEventSource<SKSE::CrosshairRefEvent>*) override {
        auto* ui=RE::UI::GetSingleton();if (event && ui && !ui->IsMenuOpen(RE::Console::MENU_NAME)) debugTarget=event->crosshairRef ? event->crosshairRef->CreateRefHandle() : RE::ObjectRefHandle{};
        return RE::BSEventNotifyControl::kContinue;
    }
    void flush() {
        std::vector<DeliveryNotice> completed;
        std::vector<Movement> moved;std::vector<unsigned> invested;std::vector<UniqueChange> changed;
        {std::scoped_lock lock(inboxMutex);moved.swap(movements);invested.swap(investments);changed.swap(uniqueChanges);completed.swap(deliveryNotices);}
        for (const auto& notice:completed) deliveryResult(notice);
        for (auto e:changed) {
            auto it=std::find_if(books.begin(),books.end(),[e](auto* b) {return b && b->GetFormID()==e.book;});if (it==books.end()) continue;
            const auto index=unsigned(it-books.begin())%textCount;
            for (auto& [id,n]:data.notes) if (bookFor(n)==*it && rebind(n,index,e.oldBase,e.oldUID,e.newBase,e.newUID)) break;
        }
        for (auto e:moved) {
            auto it=std::find_if(books.begin(),books.end(),[e](auto* b) {return b && b->GetFormID()==e.book;});if (it==books.end()) continue;
            auto* destination=e.newOwner ? RE::TESForm::LookupByID<RE::TESObjectREFR>(e.newOwner) : nullptr;
            for (auto& [id,n]:data.notes) if (n.text==unsigned(it-books.begin())%textCount && bookFor(n)==*it && n.owner==e.oldOwner && (n.extraUID==e.uid || (destination && find(destination,n)))) {
                n.owner=e.newOwner;if (e.newOwner==0x14) n.purpose=Purpose::Delivered;
            }
        }
        for (auto id:invested) {auto& s=data.actors[id];if (!s.investmentSeen) {s.investmentSeen=true;s.investmentDue=now()+data.random.days(2,3);auto* a=RE::TESForm::LookupByID<RE::Actor>(id);auto* cell=a ? a->GetParentCell() : nullptr;s.shop=cell && cell->IsInteriorCell() ? cell->GetName() : a ? std::string(a->GetName())+"'s trade" : "the shop";logger::info("Investment observed for {:08X}",id);}}
    }
    void delayedLetters() {
        std::vector<unsigned> drafts;for (const auto& [id,n]:data.notes) if (n.purpose==Purpose::HateDraft && now()>=n.due) drafts.push_back(id);
        for (auto id:drafts) {auto& n=data.notes.at(id);auto* actor=RE::TESForm::LookupByID<RE::Actor>(n.sender);
            if (!readyHate(n,now(),actor && find(actor,n),actor && !actor->IsDead())) {n.purpose=Purpose::Delivered;continue;}sendPlayer(id,actor);
        }
        std::vector<unsigned> due;for (auto& [id,s]:data.actors) if (s.investmentDue>=0 && now()>=s.investmentDue) {s.investmentDue=-1;if (data.random.chance(20)) due.push_back(id);}
        for (auto id:due) if (auto* a=RE::TESForm::LookupByID<RE::Actor>(id);a && !a->IsDead()) {
            std::string shop=data.actors.at(id).shop;
            directPlayerLetter(10,a,std::string(a->GetName())+", "+shop,shop);
        }
    }
    void bundles() {
        if (!courier || now()<data.bundleDue) return;data.bundleDue=now()+data.random.days(3,5);
        std::vector<unsigned> old;for (const auto& [id,n]:data.notes) if (n.purpose==Purpose::Bundle && n.owner==courier->GetFormID()) old.push_back(id);
        for (auto id:old) transfer(id,courier,nullptr);
        const unsigned target=2+unsigned(data.random.next()%3);
        for (unsigned i=0;i<target;++i) create(pick(unsigned(data.random.next()%8)),courier,Purpose::Bundle);
    }
    void deliveryResult(const DeliveryNotice& notice) {
        if (notice.note!=data.courierKey) return;
        auto it=data.notes.find(notice.note);auto* recipient=RE::TESForm::LookupByID<RE::Actor>(data.courierTarget);
        bool handed=false;
        if (notice.ready && it!=data.notes.end() && eligible(recipient) && it->second.owner==courier->GetFormID()) handed=transfer(notice.note,courier,recipient);
        if (handed) {data.actors[recipient->GetFormID()].refresh=now()+data.random.days(3,5);logger::info("Courier event delivered exact note {:08X} to {:08X}",notice.note,recipient->GetFormID());}
        else logger::debug("Courier event deferred/failed for note {:08X}; pending copy retained",notice.note);
        data.courierKey=0;data.courierTarget=0;data.sceneSeen=false;
        data.nextCourier=repeatNotes ? now() : now()+(handed ? 0.5 : 0.02);
    }
    void deliveries() {
        if (!active || !eventBridgeReady || !RE::UI::GetSingleton() || RE::UI::GetSingleton()->GameIsPaused() || data.courierKey) return;
        if (!repeatNotes && now()<data.nextCourier) return;
        auto* calendar=RE::Calendar::GetSingleton();if (!repeatNotes && (calendar->GetHour()<8 || calendar->GetHour()>=20)) return;
        std::vector<unsigned> recipients;for (const auto& [id,state]:data.actors) if (state.pocket) recipients.push_back(id);
        std::sort(recipients.begin(),recipients.end());std::rotate(recipients.begin(),std::upper_bound(recipients.begin(),recipients.end(),lastDebugRecipient),recipients.end());
        for (auto id:recipients) {
            auto& state=data.actors.at(id);auto note=data.notes.find(state.pocket);auto* actor=RE::TESForm::LookupByID<RE::Actor>(id);
            if (note==data.notes.end() || note->second.owner!=courier->GetFormID() || !routeCourier(actor) || !find(courier,note->second)) continue;
            data.courierKey=state.pocket;data.courierTarget=id;lastDebugRecipient=id;
            auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();auto handle=vm->GetObjectHandlePolicy()->GetHandleForObject(npcQuest->GetFormType(),npcQuest);
            vm->SendEvent(handle,"OnNLDeliverNote",RE::MakeFunctionArguments(static_cast<RE::Actor*>(actor),static_cast<std::int32_t>(state.pocket)));
            logger::info("Sent simple courier delivery event for note {:08X} to {:08X}",state.pocket,id);break;
        }
    }
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* e,RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
        if (e && active && e->sender==npcQuest && deliveryToken(e->numArg).has_value()) {
            const std::string_view name=e->eventName.c_str();
            if (name=="NL_NoteDeliveryReady" || name=="NL_NoteDeliveryFailed" || name=="NL_NoteDeliveryBusy") {
                {std::scoped_lock lock(inboxMutex);deliveryNotices.push_back({static_cast<unsigned>(e->numArg),name=="NL_NoteDeliveryReady"});}
                queue([this] {flush();});
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    void houseLetters(const std::vector<RE::Actor*>& actors) {
        auto* player=RE::PlayerCharacter::GetSingleton();unsigned owned=0;
        for (unsigned i=0;i<7;++i) if (homes[i]) {
            auto* owner=homes[i]->GetOwner();bool isOwned=owner==player->GetActorBase();
            if (auto* f=owner ? owner->As<RE::TESFaction>() : nullptr) isOwned=player->IsInFaction(f);
            if (isOwned) owned|=1u<<i;
        }
        if (!data.homesInitialized) {data.homesSeen=std::uint8_t(owned);data.homesInitialized=true;}
        // Existing homes count on first activation too; each home receives only one lifetime roll.
        data.homesSeen|=std::uint8_t(owned);
        for (unsigned i=0;i<7;++i) if ((data.homesSeen&(1u<<i)) && !(data.homesProcessed&(1u<<i))) {
            auto* loc=homes[i] ? settlement(homes[i]->GetLocation()) : nullptr;if (!loc) continue;
            std::vector<RE::Actor*> locals;
            for (auto* a:actors) if (eligible(a) && a->GetActorBase()->IsUnique() && settlement(a->GetEditorLocation())==loc && a!=spouse()) locals.push_back(a);
            if (locals.empty()) continue;auto* sender=locals[data.random.next()%locals.size()];
            data.homesProcessed|=std::uint8_t(1u<<i);data.houseSenders[i]=sender->GetFormID();const auto roll=data.random.next()%100;unsigned amount=roll<10 ? 2 : roll<95 ? 1 : 0;
            std::vector<unsigned> choices;for (unsigned j=0;j<224;++j) if (catalog[j].category==13+i) choices.push_back(j);
            if (data.random.chance(50)) std::reverse(choices.begin(),choices.end());
            for (unsigned j=0;j<amount;++j) {std::string sign=data.random.chance(50) ? sender->GetName() : initial(sender->GetName());if (create(choices[j],container,Purpose::ToPlayer,sender,sign)) notifyCourier();}
        }
    }
    void remoteNotes(const std::set<unsigned>& loaded) {
        auto real=std::chrono::steady_clock::now();
        if (remoteActors.empty() || real-discoveredAt>std::chrono::seconds(60)) {
            remoteActors.clear();auto [forms,lock]=RE::TESForm::GetAllForms();
            if (forms) {RE::BSReadLockGuard guard(lock.get());for (const auto& [id,form]:*forms) if (form && form->GetFormType()==RE::FormType::ActorCharacter) remoteActors.push_back(id);}
            std::sort(remoteActors.begin(),remoteActors.end());remoteCursor=0;discoveredAt=real;
        }
        unsigned budget=std::min<unsigned>(32,unsigned(remoteActors.size()));
        for (unsigned i=0;i<budget;++i) {if (remoteCursor>=remoteActors.size()) remoteCursor=0;auto id=remoteActors[remoteCursor++];if (!loaded.contains(id)) if (auto* actor=RE::TESForm::LookupByID<RE::Actor>(id);eligible(actor)) visit(actor,false);}
    }
    void scan() {
        refreshEventBridge();flush();auto* player=RE::PlayerCharacter::GetSingleton();if (!player || !player->Is3DLoaded() || !RE::UI::GetSingleton() || RE::UI::GetSingleton()->GameIsPaused()) return;
        if (itemCount->value>0) notificationOutstanding=false;
        else if (std::any_of(data.notes.begin(),data.notes.end(),[this](const auto& row) {return row.second.purpose==Purpose::ToPlayer && row.second.owner==container->GetFormID();})) notifyCourier();
        std::vector<RE::Actor*> actors;RE::ProcessLists::GetSingleton()->ForEachHighActor([&](RE::Actor* a) {actors.push_back(a);return RE::BSContainer::ForEachResult::kContinue;});
        std::set<unsigned> loaded;for (auto* a:actors) {loaded.insert(a->GetFormID());visit(a);}
        remoteNotes(loaded);
        if (auto* s=spouse();eligible(s) && std::find(actors.begin(),actors.end(),s)==actors.end()) playerLetters(s,data.actors[s->GetFormID()],now());
        delayedLetters();bundles();deliveries();houseLetters(actors);
    }
    std::string description(RE::TESObjectBOOK* b,RE::ExtraDataList* extras) const {
        auto it=std::find(books.begin(),books.end(),b);if (it==books.end() || !extras) return {};
        auto* uid=extras->GetByType<RE::ExtraUniqueID>();if (!uid) return {};
        auto n=std::find_if(data.notes.begin(),data.notes.end(),[&](const auto& row) {return row.second.text==unsigned(it-books.begin())%textCount && bookFor(row.second)==b && row.second.extraBase==uid->baseID && row.second.extraUID==uid->uniqueID;});if (n==data.notes.end()) return {};
        std::string s="<font face='$HandwrittenFont'><p align='left'>"+escaped(n->second.body);
        s+=signatureFooter(escaped(n->second.signature));return s+"</p></font>";
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESObjectLoadedEvent* e,RE::BSTEventSource<RE::TESObjectLoadedEvent>*) override {if (e && e->loaded && active) {auto id=e->formID;queue([this,id] {visit(RE::TESForm::LookupByID<RE::Actor>(id));});}return RE::BSEventNotifyControl::kContinue;}
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* e,RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
        if (e && active && e->itemCount>0 && std::any_of(books.begin(),books.end(),[e](auto* b) {return b && b->GetFormID()==e->baseObj;})) {std::scoped_lock lock(inboxMutex);movements.push_back({e->baseObj,e->uniqueID,e->oldContainer,e->newContainer});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESUniqueIDChangeEvent* e,RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*) override {
        if (e && active && std::any_of(books.begin(),books.end(),[e](auto* b) {return b && b->GetFormID()==e->objectID;})) {std::scoped_lock lock(inboxMutex);uniqueChanges.push_back({e->objectID,e->oldBaseID,e->newBaseID,e->oldUniqueID,e->newUniqueID});}return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESTrackedStatsEvent* e,RE::BSTEventSource<RE::TESTrackedStatsEvent>*) override {
        if (e && active && e->stat=="Stores Invested In") {auto* topics=RE::MenuTopicManager::GetSingleton();auto ref=topics ? topics->speaker.get() : RE::TESObjectREFRPtr{};if (!ref && topics) ref=topics->lastSpeaker.get();auto* a=ref ? ref->As<RE::Actor>() : nullptr;if (a && eligible(a)) {std::scoped_lock lock(inboxMutex);investments.push_back(a->GetFormID());}else logger::warn("Investment stat changed without a resolvable dialogue merchant; skipped rather than guessing sender");}return RE::BSEventNotifyControl::kContinue;
    }
private:
    unsigned lastDebugRecipient=0;bool formsReady=false,notificationOutstanding=false;std::atomic_bool queued{false};std::mutex inboxMutex;std::vector<Movement> movements;std::vector<UniqueChange> uniqueChanges;std::vector<unsigned> investments;std::vector<DeliveryNotice> deliveryNotices;std::jthread worker;
};
struct DebugCommands {
    static bool toggle(const RE::SCRIPT_PARAMETER*,RE::SCRIPT_FUNCTION::ScriptData*,RE::TESObjectREFR*,RE::TESObjectREFR*,RE::Script*,RE::ScriptLocals*,double& result,std::uint32_t&) {
        auto& m=Manager::get();m.setDebug(!m.config.debug);result=m.config.debug ? 1.0 : 0.0;return true;
    }
    static bool spawn(const RE::SCRIPT_PARAMETER*,RE::SCRIPT_FUNCTION::ScriptData*,RE::TESObjectREFR*,RE::TESObjectREFR*,RE::Script*,RE::ScriptLocals*,double& result,std::uint32_t&) {
        Manager::get().debugSpawn();result=1;return true;
    }
    static bool deliver(const RE::SCRIPT_PARAMETER*,RE::SCRIPT_FUNCTION::ScriptData*,RE::TESObjectREFR*,RE::TESObjectREFR*,RE::Script*,RE::ScriptLocals*,double& result,std::uint32_t&) {Manager::get().debugSpawn(true);result=1;return true;}
    static bool repeat(const RE::SCRIPT_PARAMETER*,RE::SCRIPT_FUNCTION::ScriptData*,RE::TESObjectREFR*,RE::TESObjectREFR*,RE::Script*,RE::ScriptLocals*,double& result,std::uint32_t&) {
        auto& m=Manager::get();m.toggleRepeat();result=m.repeatNotes ? 1.0 : 0.0;return true;
    }
    static bool installOne(std::string_view slot,const char* name,const char* help,RE::SCRIPT_FUNCTION::Execute_t* execute) {
        // Reuse verified no-op developer slots, never a working command or another plugin's handler.
        auto* command=RE::SCRIPT_FUNCTION::LocateConsoleCommand(slot);if (!command || !command->executeFunction) {logger::error("Cannot register {}: unused console slot missing",name);return false;}
        const auto address=reinterpret_cast<std::uintptr_t>(command->executeFunction);const auto text=REL::Module::get().segment(REL::Segment::textx);
        if (address<text.address() || address+3>text.address()+text.size() || !unusedConsoleStub({reinterpret_cast<const std::uint8_t*>(address),3})) {logger::error("Cannot register {}: {} is not an untouched no-op slot",name,slot);return false;}
        auto replacement=*command;replacement.functionName=name;replacement.shortName="";replacement.helpString=help;replacement.referenceFunction=false;replacement.SetParameters();replacement.executeFunction=execute;replacement.conditionFunction=nullptr;
        REL::safe_write(reinterpret_cast<std::uintptr_t>(command),&replacement,sizeof(replacement));logger::info("Registered console command {} in unused {} slot",name,slot);return true;
    }
    static void install() {
        installOne("TestSeenData","NLDebug","Toggle Notes and Letters debug mode for this session",toggle);
        installOne("TestLocalMap","NLSpawnNote","Force one allowed NPC note onto the crosshair NPC (debug mode required)",spawn);
        installOne("OutputAllocations","NLDeliverNote","Force a note for the crosshair NPC and request courier delivery",deliver);
        installOne("OutputMemStats","NLRepeatNotes","Toggle zero-delay NPC note/courier repeat testing (debug mode required)",repeat);
    }
};
struct BookHook {
    static inline REL::Relocation<RE::UI_MESSAGE_RESULTS(*)(RE::BookMenu*,RE::UIMessage&)> original;
    static RE::UI_MESSAGE_RESULTS process(RE::BookMenu* self,RE::UIMessage& message) {
        // The engine populates these globals before kShow. Later/hide messages may
        // retain stale instance data; do not dereference it on those paths.
        if (Manager::get().active && message.type==RE::UI_MESSAGE_TYPE::kShow) {
            auto* book=RE::BookMenu::GetTargetForm();
            const auto& owned=Manager::get().books;
            if (book && std::find(owned.begin(),owned.end(),book)!=owned.end()) {
                logger::info("Opening note {:08X}; inventory art={}",book->GetFormID(),book->inventoryModel!=nullptr);
                auto text=Manager::get().description(book,RE::BookMenu::GetExtraList());
                if (!text.empty()) RE::BookMenu::GetDescription()=text.c_str();
                else logger::debug("No instance text available; retaining base note text");
            }
        }
        auto result=original(self,message);
        return result;
    }
    static void install() {REL::Relocation<std::uintptr_t> v{RE::VTABLE_BookMenu[0]};original=v.write_vfunc(4,process);}
};
void save(SKSE::SerializationInterface* api) {auto& m=Manager::get();m.flush();auto bytes=encode(m.data);if (!api->OpenRecord(recordID,m.valid ? 2u : 0xFFFFFFFFu) || !api->WriteRecordData(bytes.data(),std::uint32_t(bytes.size()))) logger::error("Notes save failed");}
void load(SKSE::SerializationInterface* api) {
    auto& m=Manager::get();m.reset();unsigned type,version,length;bool seen=false;
    while (api->GetNextRecordInfo(type,version,length)) {if (type!=recordID) continue;if (seen || (version!=1 && version!=2 && version!=0xFFFFFFFFu) || length>128*1024*1024) {m.valid=false;continue;}seen=true;std::vector<std::uint8_t> bytes(length);if (api->ReadRecordData(bytes.data(),length)!=length) {m.valid=false;continue;}auto decoded=decode(bytes,version==0xFFFFFFFFu ? 1u : version);if (!decoded) {m.valid=false;continue;}if (version==0xFFFFFFFFu) {if (!emptyLegacyState(*decoded)) {m.valid=false;logger::error("Nonempty invalid legacy note state refused");continue;}logger::warn("Recovered empty legacy disabled state; no notes or timers existed");}m.data=std::move(*decoded);
        auto resolve=[api](unsigned id) {unsigned out=0;if (id) api->ResolveFormID(id,out);return out;};
        std::unordered_map<unsigned,ActorState> remapped;for (auto [id,s]:m.data.actors) if (auto newID=resolve(id)) remapped.emplace(newID,s);m.data.actors=std::move(remapped);
        for (auto& [id,n]:m.data.notes) {n.owner=resolve(n.owner);n.sender=resolve(n.sender);n.extraBase=resolve(n.extraBase);}
        m.data.courierTarget=resolve(m.data.courierTarget);for (auto& id:m.data.houseSenders) id=resolve(id);
    }
}
void revert(SKSE::SerializationInterface*) {Manager::get().reset();}
void message(SKSE::MessagingInterface::Message* e) {
    auto& m=Manager::get();switch (e->type) {
    case SKSE::MessagingInterface::kDataLoaded:m.initialize();BookHook::install();DebugCommands::install();break;
    case SKSE::MessagingInterface::kPreLoadGame:m.repeatNotes=false;m.debugTarget={};m.active=false;++m.session;break;
    case SKSE::MessagingInterface::kPostLoadGame:if (e->data) m.enter();break;
    case SKSE::MessagingInterface::kNewGame:m.reset();m.enter();break;
    default:break;
    }
}
}
SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);if (auto path=SKSE::log::log_directory()) {auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*path/"NotesAndLetters.log").string(),true);spdlog::set_default_logger(std::make_shared<spdlog::logger>("NotesAndLetters",std::move(sink)));spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);}
    logger::info("Notes and Letters 0.1.5 development build; runtime {}",REL::Module::get().version().string());
    const auto version=REL::Module::get().version();const std::set<unsigned> ae{317,318,323,342,353,629,640,659,1130,1170,1179};
    if (!(version.major()==1 && ((version.minor()==5 && version.patch()==97) || (version.minor()==6 && ae.contains(version.patch())) || (version.minor()==7 && (version.patch()==99 || version.patch()==104))))) {logger::error("Unsupported runtime");return false;}
    auto* api=SKSE::GetSerializationInterface();if (!api || !SKSE::GetTaskInterface()) return false;
    api->SetUniqueID(saveID);api->SetSaveCallback(save);api->SetLoadCallback(load);api->SetRevertCallback(revert);return SKSE::GetMessagingInterface()->RegisterListener(message);
}
