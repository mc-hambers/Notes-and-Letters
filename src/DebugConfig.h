#pragma once
#include <istream>
#include <string>
#include <string_view>
#include <algorithm>
#include <span>
#include <cstdint>
#include <cctype>
namespace NL {
struct Config {bool debug=false;};
inline Config readConfig(std::istream& input) {
    Config config;std::string line,section;unsigned lines=0;
    auto clean=[](std::string text) {auto comment=text.find_first_of(";#");if (comment!=text.npos) text.resize(comment);auto first=text.find_first_not_of(" \t\r");if (first==text.npos) return std::string{};auto last=text.find_last_not_of(" \t\r");text=text.substr(first,last-first+1);std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c) {return char(std::tolower(c));});return text;};
    while (lines++<1024 && std::getline(input,line)) {
        if (line.size()>4096) continue;line=clean(line);if (line.empty()) continue;
        if (line.front()=='[' && line.back()==']') {section=line.substr(1,line.size()-2);continue;}
        auto equals=line.find('=');if (equals==line.npos) continue;auto key=clean(line.substr(0,equals)),value=clean(line.substr(equals+1));
        if (value!="true" && value!="false" && value!="1" && value!="0") continue;bool enabled=value=="true" || value=="1";
        if (section=="debug" && key=="enabled") config.debug=enabled;
    }
    return config;
}
inline bool unusedConsoleStub(std::span<const std::uint8_t> bytes) {return bytes.size()>=3 && bytes[0]==0xB0 && bytes[1]==0x01 && bytes[2]==0xC3;}
inline bool debugSpawnAllowed(bool enabled,bool active,bool eligible,bool outstanding) {return enabled && active && eligible && !outstanding;}
}
