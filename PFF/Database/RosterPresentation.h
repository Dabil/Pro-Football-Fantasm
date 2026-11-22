#pragma once
#include "GCE/Database/WorldRoster.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <map>

namespace PffRoster
{
inline std::string group(const std::string& p)
{
    if(p=="QB"||p=="WR"||p=="TE"||p=="K"||p=="P") return p;
    if(p=="RB"||p=="FB"||p=="HB"||p=="TB") return "RB";
    if(p=="OL"||p=="C"||p=="G"||p=="T"||p=="OC"||p=="OG"||p=="OT"||
        p=="LG"||p=="RG"||p=="LT"||p=="RT") return "OL";
    // Combined source positions belong in OL only when every component does.
    // Grouping is for display/filtering; retain the original position/depth.
    const auto separator=p.find('-');
    if(separator!=std::string::npos&&group(p.substr(0,separator))=="OL"&&
        group(p.substr(separator+1))=="OL") return "OL";
    if(p=="DL"||p=="DE"||p=="DT"||p=="NT"||p=="LE"||p=="RE"||p=="LDE"||p=="RDE"||p=="LDT"||p=="RDT") return "DL";
    if(p=="LB"||p=="ILB"||p=="OLB"||p=="MLB"||p=="LLB"||p=="RLB") return "LB";
    if(p=="DB"||p=="CB"||p=="S"||p=="FS"||p=="SS"||p=="LCB"||p=="RCB") return "DB";
    return "Other";
}
inline int groupOrder(const std::string& g)
{
    const std::vector<std::string> order{"QB","WR","TE","RB","OL","DL","LB","DB","K","P","Other"};
    return static_cast<int>(std::find(order.begin(),order.end(),g)-order.begin());
}
inline std::string status(const WorldRosterPlayer& p)
{
    if(p.healthStatus.empty()) throw std::runtime_error("Player availability data is missing.");
    if(p.rosterStatus=="IR"||p.rosterStatus=="INJURED_RESERVE"||p.healthStatus=="IR") return "IR";
    if(p.healthStatus=="HEALTHY"||p.healthStatus=="OK") return "OK";
    if(p.healthStatus=="PROBABLE"||p.healthStatus=="P") return "P";
    if(p.healthStatus=="QUESTIONABLE"||p.healthStatus=="Q") return "Q";
    if(p.healthStatus=="OUT"||p.healthStatus=="INJURED")
    {
        if(!p.gamesRemaining||*p.gamesRemaining<1) throw std::runtime_error("Out player has no valid remaining-game count.");
        return "O"+std::to_string(*p.gamesRemaining);
    }
    throw std::runtime_error("Unsupported player availability: "+p.healthStatus);
}
inline bool matches(const WorldRosterPlayer& p,const std::string& filter)
{
    const auto g=group(p.position);
    if(filter=="All") return true;
    if(filter=="Injured") return status(p)!="OK";
    if(filter=="Offense") return groupOrder(g)<5;
    if(filter=="Defense") return g=="DL"||g=="LB"||g=="DB";
    if(filter=="SP") return g=="WR"||g=="TE"||g=="RB";
    return filter==g;
}
struct Column {std::string key,label,name;};
// Explicit First Edition vocabulary, in source card order. Technical keys
// remain visible in help; custom keys are appended rather than discarded.
inline std::vector<Column> columns(const WorldRosterPlayer& player)
{
    static const std::map<std::string,std::string> families{
        {"QUARTERBACK","PASS_QUICK PASS_SHORT PASS_MEDIUM PASS_LONG ARM_STRENGTH SECOND_RECEIVER THIRD_RECEIVER BALL_HANDLING INTERCEPTION SCRAMBLE_RUN THROW_VS_RUN HURRY_UP_OFF AUDIBLE FUMBLE"},
        {"SKILL_POSITION","RUN_CUTS RUN_HOLES RUN_POWER RUN_QUICK BLOCKING BREAK_SPEED BREAK_TACKLE PASS_CROWDS PASS_HANDS PASS_MOVES PASS_PATTERNS PASSING ENDURANCE FUMBLE"},
        {"OFFENSIVE_LINEMAN","BLOCK_DRIVE BLOCK_CROSS BLOCK_PULL BLOCK_HOOK BLOCK_OPEN BLOCK_FINESSE FALSE_KEY PASS_POWER PASS_QUICK PASS_STUNTS PASS_CUT RUNNING RECEIVING FUMBLE"},
        {"DEFENDER","ATTACK_POINT LATERAL_MOVE RANGE READ_AND_REACT TACKLING FUMBLE_REC INTERCEPTION RUSH_POWER RUSH_QUICK RUSH_STUNTS SHORT_MAN SHORT_ZONE DEEP_MAN DEEP_ZONE"},
        {"KICKER","KICK_NORMAL KICK_SQUIB KICK_ONSIDES KICK_COVER RUNNING PASSING RECEIVING EXTRA_POINTS FG_1_TO_12 FG_13_TO_22 FG_23_TO_32 FG_33_TO_37 FG_38_TO_42 BLOCKED"},
        {"PUNTER","PUNT_NORMAL PUNT_COFFIN BLOCKED PUNT_COVER RUNNING PASSING RECEIVING PUNT_RETURN_A PUNT_RETURN_B KICK_RETURN_A KICK_RETURN_B KICK_RETURN_C ONSIDE_RETURN KICK_BLOCK"}};
    static const std::map<std::string,std::pair<std::string,std::string>> labels{
        {"PASS_QUICK",{"PQ","Quick passing"}},{"PASS_SHORT",{"PS","Short passing"}},
        {"PASS_MEDIUM",{"PM","Medium passing"}},{"PASS_LONG",{"PL","Long passing"}},
        {"ARM_STRENGTH",{"ARM","Arm strength"}},{"SECOND_RECEIVER",{"2RC","Second receiver"}},
        {"THIRD_RECEIVER",{"3RC","Third receiver"}},{"BALL_HANDLING",{"BH","Ball handling"}},
        {"INTERCEPTION",{"INT","Interception"}},{"SCRAMBLE_RUN",{"SCR","Scramble/run"}},
        {"THROW_VS_RUN",{"TVR","Throw versus rush (source: THROW_VS_RUN)"}},
        {"HURRY_UP_OFF",{"HUP","Hurry-up offense"}},{"AUDIBLE",{"AUD","Audible"}},{"FUMBLE",{"FUM","Fumble"}},
        {"RUN_CUTS",{"RC","Running: cuts"}},{"RUN_HOLES",{"RH","Running: holes"}},
        {"RUN_POWER",{"RP","Running: power"}},{"RUN_QUICK",{"RQ","Running: quick"}},
        {"BLOCKING",{"BLK","Blocking"}},{"BREAK_SPEED",{"BSP","Breakaway speed"}},
        {"BREAK_TACKLE",{"BTK","Break tackle"}},{"PASS_CROWDS",{"PCR","Receiving: crowds"}},
        {"PASS_HANDS",{"PH","Receiving: hands"}},{"PASS_MOVES",{"PMV","Receiving: moves"}},
        {"PASS_PATTERNS",{"PPT","Receiving: patterns"}},{"PASSING",{"PAS","Passing"}},
        {"ENDURANCE",{"END","Endurance"}},{"BLOCK_DRIVE",{"BD","Drive block"}},
        {"BLOCK_CROSS",{"BC","Cross block"}},{"BLOCK_PULL",{"BP","Pull block"}},
        {"BLOCK_HOOK",{"BH","Hook block"}},{"BLOCK_OPEN",{"BO","Open-field block"}},
        {"BLOCK_FINESSE",{"BF","Finesse block"}},{"FALSE_KEY",{"FK","False key"}},
        {"PASS_POWER",{"PP","Pass protection: power"}},{"PASS_STUNTS",{"PST","Pass protection: stunts"}},
        {"PASS_CUT",{"PC","Pass protection: cut"}},{"RUNNING",{"RUN","Running"}},
        {"RECEIVING",{"REC","Receiving"}},{"ATTACK_POINT",{"AP","Attack point"}},
        {"LATERAL_MOVE",{"LM","Lateral movement"}},{"RANGE",{"RNG","Range"}},
        {"READ_AND_REACT",{"RR","Read and react"}},{"TACKLING",{"TKL","Tackling"}},
        {"FUMBLE_REC",{"FR","Fumble recovery"}},{"RUSH_POWER",{"RP","Pass rush: power"}},
        {"RUSH_QUICK",{"RQ","Pass rush: quick"}},{"RUSH_STUNTS",{"RST","Pass rush: stunts"}},
        {"SHORT_MAN",{"SM","Short man coverage"}},{"SHORT_ZONE",{"SZ","Short zone coverage"}},
        {"DEEP_MAN",{"DM","Deep man coverage"}},{"DEEP_ZONE",{"DZ","Deep zone coverage"}},
        {"KICK_NORMAL",{"KN","Normal kickoff"}},{"KICK_SQUIB",{"KS","Squib kickoff"}},
        {"KICK_ONSIDES",{"KO","Onside kickoff"}},{"KICK_COVER",{"KC","Kick coverage"}},
        {"EXTRA_POINTS",{"XP","Extra points"}},{"BLOCKED",{"BLK","Blocked kick/punt"}},
        {"FG_1_TO_12",{"F12","Field goal rating: source band 1 to 12"}},
        {"FG_13_TO_22",{"F22","Field goal rating: source band 13 to 22"}},
        {"FG_23_TO_32",{"F32","Field goal rating: source band 23 to 32"}},
        {"FG_33_TO_37",{"F37","Field goal rating: source band 33 to 37"}},
        {"FG_38_TO_42",{"F42","Field goal rating: source band 38 to 42"}},
        {"PUNT_NORMAL",{"PN","Normal punt"}},{"PUNT_COFFIN",{"PCO","Coffin-corner punt"}},
        {"PUNT_COVER",{"PCV","Punt coverage"}},{"PUNT_RETURN_A",{"PRA","Punt return A"}},
        {"PUNT_RETURN_B",{"PRB","Punt return B"}},{"KICK_RETURN_A",{"KRA","Kick return A"}},
        {"KICK_RETURN_B",{"KRB","Kick return B"}},{"KICK_RETURN_C",{"KRC","Kick return C"}},
        {"ONSIDE_RETURN",{"OR","Onside return"}},{"KICK_BLOCK",{"KB","Kick block"}}};
    std::vector<Column> result;
    auto add=[&](const std::string& key){
        if(std::any_of(result.begin(),result.end(),[&](const auto& c){return c.key==key;}))return;
        if(std::none_of(player.ratings.begin(),player.ratings.end(),[&](const auto& r){return r.key==key;}))return;
        const auto label=labels.find(key);
        auto name=label==labels.end()?key:label->second.second;
        if(player.cardType=="OFFENSIVE_LINEMAN"&&key=="PASS_QUICK")name="Pass protection: quick";
        result.push_back({key,label==labels.end()?key.substr(0,3):label->second.first,name});
    };
    const auto f=families.find(player.cardType);
    if(f!=families.end()){std::istringstream stream(f->second);std::string key;while(stream>>key)add(key);}
    for(const auto& rating:player.ratings)add(rating.key);
    return result;
}
inline std::string number(double v){std::ostringstream s;s<<v;return s.str();}
inline std::pair<std::string,std::string> rating(const WorldRosterPlayer& p,const Column& c)
{
    const auto value=std::find_if(p.ratings.begin(),p.ratings.end(),[&](const auto& r){return r.key==c.key;});
    if(value==p.ratings.end()) return {"--",{}};
    double current=value->baseline;
    bool storedInjury=false,modified=false;
    const bool exempt=c.key=="ENDURANCE"||c.key=="INJURY"||c.key=="INJ";
    for(const auto& m:p.modifiers)if(m.ratingKey==c.key)
    {
        const bool injury=m.reason=="INJURY"||m.reason=="PROBABLE"||m.reason=="QUESTIONABLE"||m.reason=="P"||m.reason=="Q";
        if(exempt&&injury)continue;
        current+=m.adjustment;
        modified|=m.adjustment!=0;
        storedInjury|=injury;
    }
    const auto state=status(p);
    if(!storedInjury && !exempt && (state=="P"||state=="Q"))
    { const int penalty=state=="P"?-1:-2;current+=penalty;modified=true; }
    return {number(current),modified?"Original: "+number(value->baseline):std::string{}};
}
} // namespace PffRoster
