#include "Core.h"
#include "Catalog.h"
#include "DebugConfig.h"
#include "RuntimeExtraData.h"
#include "FormResolution.h"
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace NL;
unsigned checks=0;
void require(bool ok) {++checks;if (!ok) throw std::runtime_error("Note behavior assertion "+std::to_string(checks));}
struct MockReference {static constexpr unsigned FORMTYPE=1;};
struct MockForm {unsigned type=2;MockReference reference;template<class T>T* As() {return &reference;}};
struct MockHandler {
    MockForm actor;MockForm* LookupForm(unsigned id,const char*) {return id ? &actor : nullptr;}
    template<class T>T* LookupForm(unsigned id,const char* name) {auto* form=LookupForm(id,name);return form && form->type==T::FORMTYPE ? form->template As<T>() : nullptr;}
};
int main() {
    for (unsigned oldPercent:{1u,2u,3u,5u,20u,100u}) {
        Random reduced{42+oldPercent};unsigned successes=0;
        for (unsigned i=0;i<200000;++i) successes+=reduced.letterChance(oldPercent);
        const double expected=1000.0*oldPercent;require(std::abs(successes-expected)<expected*0.15+10);
    }
    {Random random{73};std::array<unsigned,3> amounts{};for(unsigned i=0;i<200000;++i) ++amounts[houseLetterCount(random)];require(amounts[0]>103000 && amounts[0]<107000);require(amounts[1]>83000 && amounts[1]<87000);require(amounts[2]>9000 && amounts[2]<11000);}
    {
        std::istringstream ini("[General]\nUseNPCCourier=false\n[Debug]\nEnabled=TRUE ; test\n");auto config=readConfig(ini);require(!config.useNPCCourier && config.debug);
        std::istringstream invalid("[General]\nUseNPCCourier=invalid\n[Debug]\nEnabled=maybe\n");config=readConfig(invalid);require(config.useNPCCourier && !config.debug);
        require(debugSpawnAllowed(true,true,true,false));require(!debugSpawnAllowed(false,true,true,false));require(!debugSpawnAllowed(true,false,true,false));require(!debugSpawnAllowed(true,true,false,false));require(!debugSpawnAllowed(true,true,true,true));
        require(useCourier(true,true,true,true));for (unsigned mask=0;mask<15;++mask) require(!useCourier(mask&1,mask&2,mask&4,mask&8));
        std::array<std::uint8_t,3> stub{0xB0,0x01,0xC3},working{0x48,0x83,0xEC};require(unusedConsoleStub(stub));require(!unusedConsoleStub(working));require(!unusedConsoleStub(std::span(stub).first(2)));
        struct Facade {std::uint8_t bytes[12];};std::array<std::uint8_t,64> memory;memory.fill(0xA5);std::size_t requested=0;
        auto* data=allocateRuntimeExtraData<Facade>([&](std::size_t bytes)->void* {requested=bytes;return memory.data();},[](void* storage) {std::fill_n(static_cast<std::uint8_t*>(storage),32,0);});require(data && requested==32);require(std::all_of(memory.begin()+32,memory.end(),[](auto b) {return b==0xA5;}));
        bool constructed=false;require(!allocateRuntimeExtraData<Facade>([](std::size_t)->void* {return nullptr;},[&](void*) {constructed=true;}));require(!constructed);
        for (unsigned role=0;role<=Settler;++role) {Random r{999+role};for (unsigned i=0;i<1000;++i) require(categoryWeight(Role(role),forcedNPCCategory(r,Role(role)))>0);}
    }
    {
        MockHandler handler;require(!handler.LookupForm<MockReference>(1,"Skyrim.esm"));require(resolveReference<MockReference>(&handler,1)==&handler.actor.reference);require(!resolveReference<MockReference>(&handler,0));
        ActorState state;state.refresh=200;require(!npcRefreshDue(state,100,false));require(npcRefreshDue(state,100,true));require(state.refresh==200);require(npcRefreshDue(state,200,false));
        Data blank;require(emptyLegacyState(blank));auto copy=decode(encode(blank));require(copy && emptyLegacyState(*copy));
        blank.actors[1]={};require(!emptyLegacyState(blank));blank.actors.clear();blank.serials[0]=1;require(!emptyLegacyState(blank));blank.serials[0]=0;blank.bundleDue=10;require(!emptyLegacyState(blank));blank.bundleDue=-1;blank.homesInitialized=true;require(!emptyLegacyState(blank));blank.homesInitialized=false;blank.courierTarget=1;require(!emptyLegacyState(blank));
    }
    {
        // A real schema-1 payload contains no model byte or helper-route flag.
        Writer legacy;legacy.number<std::uint64_t>(123);
        for (unsigned i=0;i<224;++i) legacy.number<std::uint16_t>(i==9 ? 1 : 0);
        legacy.number<std::uint32_t>(0);legacy.number<std::uint32_t>(1);
        legacy.number<std::uint32_t>((9u<<16)|1);legacy.number<std::uint16_t>(9);legacy.number<std::uint16_t>(1);
        for (unsigned id:{0x100u,0x100u,0x200u}) legacy.number<std::uint32_t>(id);
        legacy.number<std::uint16_t>(1);legacy.real(0);legacy.number<std::uint8_t>(0);legacy.string("Preserved text");legacy.string("A.");
        legacy.number<std::uint32_t>(0);legacy.number<std::uint32_t>(0);legacy.number<std::uint8_t>(0);legacy.real(-1);legacy.real(-1);
        for (unsigned i=0;i<3;++i) legacy.number<std::uint8_t>(0);for (unsigned i=0;i<7;++i) legacy.number<std::uint32_t>(0);
        auto migrated=decode(legacy.bytes,1);require(migrated.has_value());require(migrated->notes.begin()->second.model==0);require(migrated->notes.begin()->second.body=="Preserved text");require(migrated->notes.begin()->second.signature=="A.");require(decode(encode(*migrated)).has_value());
        require(!decode(legacy.bytes,99));
        for (unsigned model=0;model<modelCount;++model) {
            auto value=*migrated;value.notes.begin()->second.model=std::uint8_t(model);auto restored=decode(encode(value));require(restored && restored->notes.begin()->second.model==model);
        }
        migrated->notes.begin()->second.model=4;require(!decode(encode(*migrated)));
        Data helper;helper.helperDelivery=true;require(!decode(encode(helper))); // A route cannot outlive its tracked note/recipient.
    }
    require(deliveryToken(float((223u<<16)|65535u))==((223u<<16)|65535u));
    require(!deliveryToken(0));require(!deliveryToken(-1));require(!deliveryToken(1.5f));require(!deliveryToken(std::numeric_limits<float>::infinity()));require(!deliveryToken(float(224u<<16)));
    require(signatureFooter("A.")=="<br><br>A.");require(signatureFooter("").empty());
    Data d;d.random.value=1234;
    require(eligibleToRefresh(ActorState{},100));ActorState a;a.refresh=104;require(!eligibleToRefresh(a,103.99));require(eligibleToRefresh(a,104));
    for (unsigned i=0;i<10000;++i) {auto days=d.random.days(3,5);require(days>=3 && days<5);days=d.random.days(2,3);require(days>=2 && days<3);}
    require(categoryWeight(Beggar,0)==1);require(categoryWeight(Bandit,4)==1);require(categoryWeight(Noble,0)==3);require(categoryWeight(Guard,2)==1);require(categoryWeight(Merchant,5)==3);require(categoryWeight(Bandit,5)==0);
    for (unsigned role=0;role<=Settler;++role) {
        Random r{100+role};unsigned successes=0;
        for (unsigned i=0;i<100000;++i) if (auto c=rollNPC(r,Role(role))) {++successes;require(categoryWeight(Role(role),*c)>0);}
        require(successes>400 && successes<1750);
    }
    for (const auto& t:catalog) {if (t.category==7) require(t.signature.empty());if (t.category==12) require(t.body.starts_with("{player_name},\n\n"));}
    Instance n;n.text=210;n.model=3;n.uid=12;n.owner=0x100;n.sender=0x100;n.purpose=Purpose::HateDraft;n.due=4.7;n.body="Name,\n\nI dislike you.";n.signature="A.";
    d.serials[n.text]=12;d.notes[key(n)]=n;d.actors[0x100]=a;d.actors[0x100].pocket=key(n);d.courierKey=key(n);d.courierTarget=0x100;d.sceneSeen=true;d.houseSenders[0]=0x100;d.homesInitialized=true;
    require(!readyHate(n,4.6,true,true));require(readyHate(n,4.7,true,true));require(!readyHate(n,9,false,true));require(!readyHate(n,9,true,false));
    auto stolen=n;stolen.owner=0x14;require(!readyHate(stolen,9,true,true));stolen=n;stolen.purpose=Purpose::Delivered;require(!readyHate(stolen,9,true,true));
    auto moved=n;moved.extraBase=0x100;moved.extraUID=8;const auto oldKey=key(moved);
    require(!rebind(moved,210,0x100,7,0x14,22));require(rebind(moved,210,0x100,8,0x14,22));require(key(moved)==oldKey);require(moved.signature==n.signature);require(moved.body==n.body);
    require(!canStageHate(d,0x100));require(canStageHate(d,0x200));
    auto bytes=encode(d);auto copy=decode(bytes);require(copy.has_value());require(copy->notes.at(key(n)).model==3);require(copy->actors.at(0x100).refresh==104);require(copy->notes.at(key(n)).signature=="A.");require(copy->notes.at(key(n)).body==n.body);require(copy->courierKey==key(n));require(copy->sceneSeen);require(copy->houseSenders[0]==0x100);
    for (unsigned i=0;i<bytes.size();++i) require(!decode(std::span(bytes).first(i)));
    auto extra=bytes;extra.push_back(0);require(!decode(extra));
    d.notes.at(key(n)).purpose=Purpose::Delivered;require(canStageHate(d,0x100));d.notes.at(key(n)).purpose=Purpose::ToPlayer;require(!canStageHate(d,0x100));
    d.serials[n.text]=11;require(!decode(encode(d)));
    std::cout<<"Passed "<<checks<<" note behavior checks\n";
}
