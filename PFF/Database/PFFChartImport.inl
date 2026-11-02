// Private PFF chart import implementation. Included inside PFFDatabase.cpp's
// anonymous namespace so it shares the transactional importer infrastructure.
// Values remain authored tokens; importing a script never certifies execution.
struct ChartNode
{
    std::string type;
    std::size_t line = 0, endLine = 0;
    int parent = -1;
    std::map<std::string, std::vector<std::string>> fields;
    std::map<std::string, std::string> scripts;
};

struct ChartFormat
{
    std::string family;
    std::set<std::string> blocks;
};

const std::map<std::string, ChartFormat>& chartFormats()
{
    static const std::map<std::string, ChartFormat> formats{
        {"breakaway.pffdata", {"BREAKAWAY", {"CHART_ROW"}}},
        {"fumble.pffdata", {"FUMBLE", {"CHART_ROW"}}},
        {"interception_point.pffdata", {"INTERCEPTIONPOINT", {"CHART_ROW"}}},
        {"interception_return.pffdata", {"INTERCEPTIONRETURN", {"CHART_ROW"}}},
        {"scramble.pffdata", {"SCRAMBLE", {"CHART_ROW"}}},
        {"Injury_Chart.pffdata", {"INJURY", {"INJURY_CHART"}}},
        {"endurance.pffdata", {"ENDURANCE", {"CHART","ENDURANCE_ROW"}}},
        {"penalty.pffdata", {"PENALTY", {"CHART","PENALTY_RESULT"}}},
        {"special_teams.pffdata", {"SPECIAL_TEAMS", {"SPECIAL_TEAM_PLAY","DISTANCE_ROW","RETURN_ROW"}}},
        {"weather.pffdata", {"WEATHER", {"CHART","WEATHER_PROFILE","WEATHER_MONTH"}}},
        {"weather_turf_effects.pffdata", {"WEATHER_TURF_EFFECTS", {"CHART","WEATHER_EFFECT","MODIFIER"}}},
        {"wild_play_running.pffdata", {"WILD_PLAY_RUNNING", {"CHART","WILD_PLAY_RESULT"}}},
        {"wild_play_passing.pffdata", {"WILD_PLAY_PASSING", {"CHART","WILD_PLAY_RESULT"}}},
        {"wild_play_kicking.pffdata", {"WILD_PLAY_KICKING", {"CHART","WILD_PLAY_RESULT"}}},
        {"solitaire_play_calling.pffdata", {"SOLITAIRE", {
            "SITUATIONS","SITUATION","PLAY_CALL_TABLES","PLAY_CALL_ROW",
            "PLAY_SELECTION_TABLES","PLAY_SELECTION_ROW","PLAYER_SELECTION_TABLE",
            "PLAYER_SELECTION_ROW","GAME_PLAN","GAME_PLAN_CHECKPOINT","GAME_PLAN_DICE_MODIFIER"}}}
    };
    return formats;
}

void createChartSchema(sqlite3* db)
{
    exec(db, R"SQL(
CREATE TABLE ChartDefinitions(
 ChartID INTEGER PRIMARY KEY, RulesetID INTEGER NOT NULL REFERENCES Rulesets(RulesetID),
 Family TEXT NOT NULL, SourcePath TEXT NOT NULL,
 UNIQUE(RulesetID,Family), UNIQUE(RulesetID,SourcePath));
CREATE TABLE ChartRecords(
 RecordID INTEGER PRIMARY KEY, ChartID INTEGER NOT NULL REFERENCES ChartDefinitions(ChartID),
 ParentRecordID INTEGER REFERENCES ChartRecords(RecordID), RecordType TEXT NOT NULL,
 RecordKey TEXT NOT NULL, RecordOrder INTEGER NOT NULL, SourceLine INTEGER NOT NULL,
 SourceEndLine INTEGER NOT NULL, UNIQUE(ChartID,RecordOrder));
CREATE TABLE ChartValues(
 RecordID INTEGER NOT NULL REFERENCES ChartRecords(RecordID), FieldKey TEXT NOT NULL,
 ValueIndex INTEGER NOT NULL, ValueToken TEXT NOT NULL,
 PRIMARY KEY(RecordID,FieldKey,ValueIndex));
CREATE TABLE ChartScripts(
 RecordID INTEGER NOT NULL REFERENCES ChartRecords(RecordID), ScriptKind TEXT NOT NULL,
 ScriptText TEXT NOT NULL, ExecutionStatus TEXT NOT NULL CHECK(ExecutionStatus IN ('DRAFT','UNCOMPILED')),
 PRIMARY KEY(RecordID,ScriptKind));
CREATE INDEX IX_ChartRecords_Type ON ChartRecords(ChartID,RecordType,RecordKey);
)SQL");
}

std::string chartValue(const ChartNode& node, const std::string& key)
{
    const auto found = node.fields.find(key);
    return found == node.fields.end() ? std::string{} : found->second.front();
}

std::vector<ChartNode> parseChart(const std::filesystem::path& file, const ChartFormat& format)
{
    // Allowed fields are explicit, so misspellings/unsupported source changes
    // cannot disappear silently. Requirements below are record/family specific.
    static const std::map<std::string, std::string> fields{
        {"CHART", "CHART_KEY DISPLAY_NAME CHART_TYPE PROVENANCE_TYPE SOURCE_FILE SOURCE_SHEET ROW_COUNT DIE_TYPE ROLL_MIN ROLL_MAX CONTEXT_COUNT RESULT_COUNT EFFECT_COUNT PROFILE_COUNT MONTH_COUNT WILD_PLAY_CATEGORY WILD_PLAY_DATA_VERSION SCRIPT_LANGUAGE DEFAULT_SCRIPT_VERSION RULESET_SELECTOR_ALIAS"},
        {"CHART_ROW", "CHART_TYPE SOURCE_ROW SOURCE_CSV_LINE SOURCE_CODE SOURCE_RESULT INTERCEPTION_RATING SOURCE_RATING SCREEN QUICK SHORT MEDIUM LONG"},
        {"INJURY_CHART", "CHART_KEY SOURCE_REFERENCE DIE ADJUSTMENT MAX_GAMES_MISSED PROVENANCE"},
        {"ENDURANCE_ROW", "END_RATING FREQUENCY_TOKEN CONSECUTIVE_TOKEN"},
        {"PENALTY_RESULT", "D20 PLAY_TYPE PENALIZED_SIDE RESULT_TYPE SOURCE_PENALTY_NAME PENALTY_KEY PENALTY_YARDS PENALTY_YARDS_TOKEN AUTOMATIC_FIRST_DOWN LOSS_OF_DOWN REKICK FLAG_DISPOSITION FLAG_THROWN PENALTY_ENFORCED"},
        {"SPECIAL_TEAM_PLAY", "PLAY_KEY NAME PLAY_TYPE SOURCE_CSV_LINE"},
        {"DISTANCE_ROW", "D20 SOURCE_CSV_LINE"}, {"RETURN_ROW", "D20 SOURCE_CSV_LINE RETURNER_CODE"},
        {"WEATHER_PROFILE", "PROFILE_KEY CITY_CODE WIND_W1 WIND_W2 WIND_W3 FOG"},
        {"WEATHER_MONTH", "MONTH TEMPERATURE_TOKEN_1 TEMPERATURE_TOKEN_2 PRECIPITATION_TOKEN"},
        {"WEATHER_EFFECT", "EFFECT_KEY SOURCE_LABEL CATEGORY"}, {"MODIFIER", "ATTRIBUTE_KEY VALUE_TOKEN"},
        {"WILD_PLAY_RESULT", "WILD_PLAY_KEY PLAY_TYPE ROLL SOURCE_TEXT SOURCE_RESULT_TOKEN SOURCE_REFERENCE SCRIPT_LANGUAGE SCRIPT_VERSION SCRIPT_STATUS INTERPRETATION_STATUS PROVENANCE_TYPE REQUIRED_WPSL_FEATURE REQUIRED_WPSL_FEATURE_AFTER_RULING IMPLEMENTATION_NOTE RULING_KEY RULING_NOTE SCRIPT_CONTENT_HASH_SHA256 DRAFT_SCRIPT_CONTENT_HASH_SHA256"},
        {"SITUATION", "SITUATION_KEY DOWN_1_DISTANCE DOWN_2_DISTANCE DOWN_3_DISTANCE DOWN_4_DISTANCE SOURCE_ROW"},
        {"PLAY_CALL_ROW", "SIDE COACHING_PHILOSOPHY RED_D10 SITUATION_A SITUATION_B SITUATION_C SOURCE_ROW"},
        {"PLAY_SELECTION_ROW", "COACHING_PHILOSOPHY BLUE_D20 RUN_PLAY_NUMBER PASS_PLAY_NUMBER SOURCE_ROW"},
        {"PLAYER_SELECTION_ROW", "WHITE_D20 RUN_ALL_PHILOSOPHIES PASS_AGGRESSIVE PASS_BALANCED PASS_CONSERVATIVE SOURCE_ROW"},
        {"GAME_PLAN_CHECKPOINT", "CHECKPOINT_KEY AHEAD_TIER_1_THRESHOLD BEHIND_TIER_1_THRESHOLD AHEAD_TIER_2_THRESHOLD BEHIND_TIER_2_THRESHOLD SOURCE_ROW"},
        {"GAME_PLAN_DICE_MODIFIER", "DICE_CONTEXT AHEAD_TIER_1_MODIFIER BEHIND_TIER_1_MODIFIER AHEAD_TIER_2_MODIFIER BEHIND_TIER_2_MODIFIER SOURCE_ROW"},
        {"SITUATIONS", ""}, {"PLAY_CALL_TABLES", ""}, {"PLAY_SELECTION_TABLES", ""},
        {"PLAYER_SELECTION_TABLE", ""}, {"GAME_PLAN", ""}
    };
    static const std::map<std::string, std::string> parents{
        {"CHART",""},{"INJURY_CHART",""},{"CHART_ROW",""},{"SPECIAL_TEAM_PLAY",""},
        {"DISTANCE_ROW","SPECIAL_TEAM_PLAY"},{"RETURN_ROW","SPECIAL_TEAM_PLAY"},
        {"ENDURANCE_ROW","CHART"},{"PENALTY_RESULT","CHART"},{"WEATHER_PROFILE","CHART"},
        {"WEATHER_MONTH","WEATHER_PROFILE"},{"WEATHER_EFFECT","CHART"},{"MODIFIER","WEATHER_EFFECT"},
        {"WILD_PLAY_RESULT","CHART"},{"SITUATIONS",""},{"SITUATION","SITUATIONS"},
        {"PLAY_CALL_TABLES",""},{"PLAY_CALL_ROW","PLAY_CALL_TABLES"},
        {"PLAY_SELECTION_TABLES",""},{"PLAY_SELECTION_ROW","PLAY_SELECTION_TABLES"},
        {"PLAYER_SELECTION_TABLE",""},{"PLAYER_SELECTION_ROW","PLAYER_SELECTION_TABLE"},
        {"GAME_PLAN",""},{"GAME_PLAN_CHECKPOINT","GAME_PLAN"},{"GAME_PLAN_DICE_MODIFIER","GAME_PLAN"}
    };
    std::ifstream input(file, std::ios::binary);
    if (!input) throw BuildException("Cannot open chart.", file);
    std::vector<ChartNode> nodes;
    std::vector<int> stack;
    std::size_t lineNumber = 0;
    std::string line, scriptKind, script;
    auto fail = [&](const std::string& message) { throw BuildException(message, file, lineNumber); };
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (lineNumber == 1 && line.compare(0,3,"\xEF\xBB\xBF") == 0) line.erase(0,3);
        const auto stripped = trim(line);
        if (!scriptKind.empty())
        {
            if (stripped == "[" + scriptKind + "_END]")
            { if(trim(script).empty()) fail("Empty script block."); nodes[stack.back()].scripts.emplace(scriptKind,script); scriptKind.clear(); script.clear(); }
            else { script += line; script += '\n'; }
            continue;
        }
        if (stripped.empty() || stripped.rfind("//",0) == 0) continue;
        if (stripped.front() != '[' || stripped.find(']') == std::string::npos) fail("Expected a chart tag.");
        const auto close = stripped.find(']');
        const auto tag = stripped.substr(1,close-1);
        const auto value = trim(stripped.substr(close+1));
        if (tag == "SCRIPT_BEGIN" || tag == "DRAFT_SCRIPT_BEGIN")
        {
            if (stack.empty() || nodes[stack.back()].type != "WILD_PLAY_RESULT" || !value.empty()) fail("Misplaced script block.");
            scriptKind = tag.substr(0,tag.size()-6);
            if (nodes[stack.back()].scripts.count(scriptKind)) fail("Duplicate script block.");
            continue;
        }
        const bool begin = tag.size() > 6 && tag.substr(tag.size()-6) == "_BEGIN";
        const bool end = tag.size() > 4 && tag.substr(tag.size()-4) == "_END";
        if (begin)
        {
            const auto type = tag.substr(0,tag.size()-6);
            if (!format.blocks.count(type) || !value.empty()) fail("Unsupported block: " + tag);
            const auto parent = stack.empty() ? std::string{} : nodes[stack.back()].type;
            if (parents.at(type) != parent) fail("Invalid parent for " + type);
            nodes.push_back({type,lineNumber,0,stack.empty() ? -1 : stack.back(),{}, {}});
            stack.push_back(static_cast<int>(nodes.size()-1));
        }
        else if (end)
        {
            if (stack.empty() || nodes[stack.back()].type != tag.substr(0,tag.size()-4) || !value.empty()) fail("Mismatched block end: " + tag);
            nodes[stack.back()].endLine = lineNumber;
            stack.pop_back();
        }
        else
        {
            if (stack.empty() || value.empty()) fail("Empty or misplaced chart field: " + tag);
            auto& node = nodes[stack.back()];
            bool outcome = false;
            if (tag.rfind("OUTCOME_",0) == 0)
            {
                const int roll = parseInt(tag.substr(8),file,lineNumber,"OUTCOME index");
                int min = node.type == "INJURY_CHART" ? 1 : 0;
                int max = node.type == "INJURY_CHART" ? 20 : format.family == "SCRAMBLE" ? 19 : 9;
                outcome = (node.type == "CHART_ROW" && format.family != "FUMBLE" && format.family != "INTERCEPTIONPOINT") ||
                    node.type == "INJURY_CHART" || node.type == "DISTANCE_ROW" || node.type == "RETURN_ROW";
                if (roll < min || roll > max) outcome = false;
            }
            if (!outcome && (" " + fields.at(node.type) + " ").find(" " + tag + " ") == std::string::npos)
                fail("Unsupported field " + tag + " in " + node.type);
            const bool repeat = tag == "REQUIRED_WPSL_FEATURE" || tag == "REQUIRED_WPSL_FEATURE_AFTER_RULING";
            if (!repeat && node.fields.count(tag)) fail("Duplicate field: " + tag);
            // Retain source wording, including trailing spaces, independently
            // from normalized keys/numeric selectors.
            node.fields[tag].push_back(tag == "SOURCE_TEXT" ? line.substr(line.find(']')+1) : value);
        }
    }
    if (input.bad() || !stack.empty() || !scriptKind.empty()) fail("Incomplete chart or script block.");
    return nodes;
}

void validateChart(std::vector<ChartNode>& nodes, const std::filesystem::path& file, const ChartFormat& format)
{
    std::set<std::string> identities;
    for (auto& node : nodes)
    {
        auto fail = [&](const std::string& message){ throw BuildException(message,file,node.line); };
        auto require = [&](const std::string& keys){
            std::istringstream list(keys); std::string key;
            while (list >> key) if (chartValue(node,key).empty()) fail("Missing " + key + " in " + node.type);
        };
        auto number = [&](const std::string& key,int low,int high){
            require(key); const int value = parseInt(chartValue(node,key),file,node.line,key.c_str());
            if(value < low || value > high) fail("Out of range " + key);
            return value;
        };
        auto choice = [&](const std::string& key,const std::string& choices){
            require(key);
            if((" "+choices+" ").find(" "+chartValue(node,key)+" ")==std::string::npos) fail("Unsupported "+key);
        };
        std::string identity;
        if(node.type == "CHART" && !chartValue(node,"CHART_TYPE").empty())
        {
            const auto expected=format.family.rfind("WILD_PLAY",0)==0?"WILD_PLAY":format.family;
            if(chartValue(node,"CHART_TYPE")!=expected) fail("CHART_TYPE does not match importer.");
        }
        if (node.type == "CHART_ROW")
        {
            require("CHART_TYPE SOURCE_ROW SOURCE_CSV_LINE");
            if(chartValue(node,"CHART_TYPE") != format.family) fail("CHART_TYPE does not match importer.");
            identity = std::to_string(number("SOURCE_ROW",1,2147483647));
            if(format.family == "FUMBLE") require("SOURCE_CODE SOURCE_RESULT");
            else if(format.family == "INTERCEPTIONPOINT") require("SCREEN QUICK SHORT MEDIUM LONG");
            else for(int i=0;i<(format.family == "SCRAMBLE" ? 20 : 10);++i) require("OUTCOME_"+std::to_string(i));
            if(format.family == "INTERCEPTIONRETURN") number("INTERCEPTION_RATING",0,2147483647);
            if(format.family == "SCRAMBLE")
            { number("SOURCE_RATING",0,2147483647); if(!identities.insert("scramble-rating:"+chartValue(node,"SOURCE_RATING")).second) fail("Duplicate scramble rating."); }
        }
        else if(node.type == "INJURY_CHART")
        { require("CHART_KEY DIE ADJUSTMENT MAX_GAMES_MISSED SOURCE_REFERENCE PROVENANCE"); for(int i=1;i<=20;++i) require("OUTCOME_"+std::to_string(i)); }
        else if(node.type == "SPECIAL_TEAM_PLAY") { require("PLAY_KEY NAME PLAY_TYPE"); choice("PLAY_TYPE","kick return"); identity=chartValue(node,"PLAY_KEY"); }
        else if(node.type == "DISTANCE_ROW" || node.type == "RETURN_ROW")
        {
            identity=std::to_string(number("D20",1,20));
            for(int i=0;i<10;++i) require("OUTCOME_"+std::to_string(i));
            if(node.type == "RETURN_ROW") require("RETURNER_CODE");
        }
        else if(node.type == "ENDURANCE_ROW") { identity=std::to_string(number("END_RATING",0,2147483647)); require("FREQUENCY_TOKEN CONSECUTIVE_TOKEN"); }
        else if(node.type == "PENALTY_RESULT")
        {
            number("D20",1,20); require("PLAY_TYPE PENALIZED_SIDE RESULT_TYPE PENALTY_KEY SOURCE_PENALTY_NAME AUTOMATIC_FIRST_DOWN LOSS_OF_DOWN REKICK");
            choice("PLAY_TYPE","RUN PASS KICK"); choice("RESULT_TYPE","PENALTY NO_PENALTY");
            choice("PENALIZED_SIDE",chartValue(node,"PLAY_TYPE")=="KICK"?"KICKING_TEAM RECEIVING_TEAM":"OFFENSE DEFENSE");
            if(chartValue(node,"PENALTY_YARDS").empty() == chartValue(node,"PENALTY_YARDS_TOKEN").empty()) fail("Expected exactly one penalty yardage representation.");
            if(!chartValue(node,"PENALTY_YARDS").empty()) number("PENALTY_YARDS",0,100);
            identity=chartValue(node,"PLAY_TYPE")+":"+chartValue(node,"PENALIZED_SIDE")+":"+chartValue(node,"D20");
            for(const auto* flag : {"AUTOMATIC_FIRST_DOWN","LOSS_OF_DOWN","REKICK","FLAG_THROWN","PENALTY_ENFORCED"})
                if(!chartValue(node,flag).empty() && chartValue(node,flag)!="TRUE" && chartValue(node,flag)!="FALSE") fail("Invalid boolean flag.");
        }
        else if(node.type == "WEATHER_PROFILE") { require("PROFILE_KEY CITY_CODE WIND_W1 WIND_W2 WIND_W3 FOG"); for(const auto* key:{"WIND_W1","WIND_W2","WIND_W3","FOG"}) number(key,0,20); identity=chartValue(node,"PROFILE_KEY"); }
        else if(node.type == "WEATHER_MONTH") { require("MONTH PRECIPITATION_TOKEN"); choice("MONTH","AUG SEP OCT NOV DEC JAN"); identity=chartValue(node,"MONTH"); }
        else if(node.type == "WEATHER_EFFECT") { require("EFFECT_KEY SOURCE_LABEL CATEGORY"); identity=chartValue(node,"EFFECT_KEY"); }
        else if(node.type == "MODIFIER") { require("ATTRIBUTE_KEY VALUE_TOKEN"); identity=chartValue(node,"ATTRIBUTE_KEY"); }
        else if(node.type == "WILD_PLAY_RESULT")
        {
            require("WILD_PLAY_KEY"); number("ROLL",1,20); identity=chartValue(node,"ROLL");
            if(!identities.insert("wild-key:"+chartValue(node,"WILD_PLAY_KEY")).second) fail("Duplicate Wild Play key.");
            if(node.scripts.empty() && chartValue(node,"SCRIPT_STATUS").empty()) fail("Wild Play requires script content or explicit script status.");
        }
        else if(node.type == "SITUATION") { require("SITUATION_KEY DOWN_1_DISTANCE DOWN_2_DISTANCE DOWN_3_DISTANCE DOWN_4_DISTANCE"); choice("SITUATION_KEY","A B C"); identity=chartValue(node,"SITUATION_KEY"); }
        else if(node.type == "PLAY_CALL_ROW") { require("SIDE COACHING_PHILOSOPHY SITUATION_A SITUATION_B SITUATION_C"); choice("SIDE","OFFENSE DEFENSE"); choice("COACHING_PHILOSOPHY","AGGRESSIVE BALANCED CONSERVATIVE"); number("RED_D10",0,9); identity=chartValue(node,"SIDE")+":"+chartValue(node,"COACHING_PHILOSOPHY")+":"+chartValue(node,"RED_D10"); }
        else if(node.type == "PLAY_SELECTION_ROW") { require("COACHING_PHILOSOPHY RUN_PLAY_NUMBER PASS_PLAY_NUMBER"); choice("COACHING_PHILOSOPHY","AGGRESSIVE BALANCED CONSERVATIVE"); number("BLUE_D20",1,20); identity=chartValue(node,"COACHING_PHILOSOPHY")+":"+chartValue(node,"BLUE_D20"); }
        else if(node.type == "PLAYER_SELECTION_ROW") { require("RUN_ALL_PHILOSOPHIES PASS_AGGRESSIVE PASS_BALANCED PASS_CONSERVATIVE"); identity=std::to_string(number("WHITE_D20",1,20)); }
        else if(node.type == "GAME_PLAN_CHECKPOINT" || node.type == "GAME_PLAN_DICE_MODIFIER")
        {
            const bool checkpoint=node.type=="GAME_PLAN_CHECKPOINT";
            const auto key=checkpoint?"CHECKPOINT_KEY":"DICE_CONTEXT"; require(key); identity=chartValue(node,key);
            for(const auto* tier:{"AHEAD_TIER_1_","BEHIND_TIER_1_","AHEAD_TIER_2_","BEHIND_TIER_2_"})
                number(std::string(tier)+(checkpoint?"THRESHOLD":"MODIFIER"),-1000,1000);
        }
        if(!identity.empty() && !identities.insert(std::to_string(node.parent)+":"+node.type+":"+identity).second) fail("Duplicate chart selector: " + identity);
        for(const auto& declared : std::map<std::string,std::string>{{"ROW_COUNT","ENDURANCE_ROW"},{"RESULT_COUNT",format.family=="PENALTY"?"PENALTY_RESULT":"WILD_PLAY_RESULT"},{"EFFECT_COUNT","WEATHER_EFFECT"},{"PROFILE_COUNT","WEATHER_PROFILE"}})
            if(!chartValue(node,declared.first).empty())
            {
                const int count=static_cast<int>(std::count_if(nodes.begin(),nodes.end(),[&](const auto& n){return n.type==declared.second;}));
                if(number(declared.first,0,10000)!=count) fail("Declared chart count mismatch.");
            }
    }
}

std::uint64_t loadChartPackage(BuildContext& context, const std::filesystem::path& file, const ChartFormat& format)
{
    auto nodes = parseChart(file,format);
    validateChart(nodes,file,format);
    Statement chart(context.db,"INSERT INTO ChartDefinitions(RulesetID,Family,SourcePath) VALUES(?,?,?);");
    bindInt64(chart.get(),1,context.rulesetId); bindText(chart.get(),2,format.family);
    bindText(chart.get(),3,pathUtf8(std::filesystem::relative(file,context.sourceRoot))); chart.stepDone(context.db); context.inserted();
    const auto chartId=lastInsertId(context.db);
    std::vector<sqlite3_int64> ids;
    for(std::size_t i=0;i<nodes.size();++i)
    {
        const auto& node=nodes[i];
        Statement record(context.db,"INSERT INTO ChartRecords(ChartID,ParentRecordID,RecordType,RecordKey,RecordOrder,SourceLine,SourceEndLine) VALUES(?,?,?,?,?,?,?);");
        bindInt64(record.get(),1,chartId);
        if(node.parent<0) sqlite3_bind_null(record.get(),2); else bindInt64(record.get(),2,ids.at(node.parent));
        bindText(record.get(),3,node.type);
        std::string key;
        for(const auto* field:{"PLAY_KEY","WILD_PLAY_KEY","PROFILE_KEY","EFFECT_KEY","ATTRIBUTE_KEY","SITUATION_KEY","CHECKPOINT_KEY","DICE_CONTEXT","D20","ROLL","SOURCE_ROW","END_RATING"})
            if(!chartValue(node,field).empty()) {key=chartValue(node,field);break;}
        bindText(record.get(),4,key); bindInt64(record.get(),5,i); bindInt64(record.get(),6,node.line); bindInt64(record.get(),7,node.endLine);
        record.stepDone(context.db); ids.push_back(lastInsertId(context.db)); context.inserted();
        for(const auto& field:node.fields) for(std::size_t n=0;n<field.second.size();++n)
        {
            Statement value(context.db,"INSERT INTO ChartValues(RecordID,FieldKey,ValueIndex,ValueToken) VALUES(?,?,?,?);");
            bindInt64(value.get(),1,ids.back()); bindText(value.get(),2,field.first); bindInt64(value.get(),3,n); bindText(value.get(),4,field.second[n]); value.stepDone(context.db); context.inserted();
        }
        for(const auto& script:node.scripts)
        {
            Statement value(context.db,"INSERT INTO ChartScripts(RecordID,ScriptKind,ScriptText,ExecutionStatus) VALUES(?,?,?,?);");
            bindInt64(value.get(),1,ids.back()); bindText(value.get(),2,script.first); bindText(value.get(),3,script.second);
            bindText(value.get(),4,script.first=="DRAFT_SCRIPT"?"DRAFT":"UNCOMPILED"); value.stepDone(context.db); context.inserted();
        }
    }
    if(format.family.rfind("WILD_PLAY",0)==0)
        context.diagnostic(DatabaseDiagnosticSeverity::Warning,"Wild Play source/scripts imported; draft/ruling statuses preserved. WPSL execution is not certified.",file);
    return nodes.size();
}
