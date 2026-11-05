// Private Core3 importer/configuration participant. CSD-001/002/010.
void buildCoreClockData(BuildContext& c,const CoreDatabaseBuildRequest& request,
                       const std::map<std::string,std::filesystem::path>& selected)
{
    namespace S=TeamDataSql;
    std::map<std::filesystem::path,std::int64_t> zoneCounts;
    std::set<std::filesystem::path> zoneOnlySources;
    for(const auto& p:request.clockProfiles)
    {
        if(p.key.empty()||p.version<=0||p.quarterSeconds<=0||p.overtimePolicyKey.empty()||
           p.timingPolicyKey.empty()||p.timingPolicyVersion<=0||p.policyContentHash.empty())
            throw BuildException("MISSING_CLOCK_PROFILE: explicit quarter duration and versioned policies are required");
        S::run(c.db,"INSERT INTO CoreGameClockProfiles(ProfileKey,ProfileVersion,RegulationPeriods,QuarterSeconds,OvertimePolicyKey,TimingPolicyKey,TimingPolicyVersion,PolicyContentHash) VALUES(?,?,4,?,?,?,?,?)",
               {p.key,std::int64_t(p.version),std::int64_t(p.quarterSeconds),p.overtimePolicyKey,p.timingPolicyKey,std::int64_t(p.timingPolicyVersion),p.policyContentHash});
        c.inserted();
    }
    for(const auto& z:request.teamHomeTimeZones)
    {
        const auto file=std::filesystem::canonical(z.sourceFile);
        if(!CoreManifest::inside(std::filesystem::canonical(c.sourceRoot),file)||!z.sourceLine||z.zoneDatabaseVersion.empty()||z.ianaZoneId.empty())
            throw BuildException("Home-city timezone requires in-package source evidence and a rules-database version",file,z.sourceLine);
        // Evidence must actually contain the named zone on the specified line.
        std::istringstream lines(CoreManifest::bytes(file)); std::string line;
        for(std::size_t i=0;i<z.sourceLine;++i) if(!std::getline(lines,line)) throw BuildException("Timezone source line is absent",file,z.sourceLine);
        if(line.find(z.ianaZoneId)==std::string::npos) throw BuildException("Timezone source line does not identify the declared zone",file,z.sourceLine);
        const auto relative=pathUtf8(std::filesystem::relative(file,c.sourceRoot));
        if(++zoneCounts[file]==1 && !S::scalar(c.db,"SELECT count(*) FROM SourceFiles WHERE RelativePath=?",{relative}))
        {zoneOnlySources.insert(file);++c.result->filesProcessed;}
        if(zoneOnlySources.count(file))registerSourceFile(c,file,"TEAM_HOME_TIME_ZONE","IMPORTED",zoneCounts[file]);
        const auto source=S::scalar(c.db,"SELECT SourceFileID FROM SourceFiles WHERE RelativePath=?",{pathUtf8(std::filesystem::relative(file,c.sourceRoot))});
        if(zoneCounts[file]==1)insertMetadata(c.db,c,"HomeZoneSource."+std::to_string(source)+".Hash",ClockTime::digest(CoreManifest::bytes(file)));
        const auto team=S::scalar(c.db,"SELECT TeamSeasonID FROM TeamSeasons WHERE TeamSeasonKey=?",{z.teamSeasonKey});
        S::run(c.db,"INSERT INTO CoreTeamHomeTimeZones VALUES(?,?,?,?,?)",{team,z.ianaZoneId,z.zoneDatabaseVersion,source,std::int64_t(z.sourceLine)}); c.inserted();
    }
    const auto found=selected.find("season_events.pffdata");
    if(found!=selected.end())
    {
        const auto& file=found->second;
        registerSourceFile(c,file,"SEASON_EVENTS","IMPORTED",0); ++c.result->filesProcessed;
        insertMetadata(c.db,c,"SeasonEvents.SourceFileHash",ClockTime::digest(CoreManifest::bytes(file)));
        std::vector<std::string> sourceLines;
        {std::istringstream input(CoreManifest::bytes(file));std::string sourceLine;while(std::getline(input,sourceLine))sourceLines.push_back(sourceLine);}
        const auto source=S::scalar(c.db,"SELECT SourceFileID FROM SourceFiles WHERE RelativePath=?",{pathUtf8(std::filesystem::relative(file,c.sourceRoot))});
        std::map<std::string,TagLine> header,record; bool begun=false,closed=false,inRecord=false;
        std::size_t line=0,count=0; std::vector<std::pair<std::string,std::string>> dependencies;
        const std::set<std::string> headerKeys={"FORMAT_VERSION","DATA_VERSION","LEAGUE","SEASON","TIME_ZONE","PROVENANCE","GENERATED_LEAGUE_DATE_POLICY"};
        const std::set<std::string> recordKeys={"EVENT_KEY","DISPLAY_NAME","EVENT_TYPE","TRIGGER","LOCAL_DATETIME","SCHEDULED_UTC","PRIORITY","ACTION","DESCRIPTION","TARGET_LEAGUE_YEAR","REQUIRES_EVENT_KEY","ACTIVITY_KEY","BOUNDARY","RELATED_EVENT_KEY","SESSION_NUMBER","FIRST_ROUND","LAST_ROUND","TIME_BASIS","EXPAND_PER","TIME_ZONE_SOURCE","MATCHUP_ROLE","CLOSE_SCOPE","ELIGIBLE_PLAYER_STATUS","ELIGIBLE_STAFF_CATEGORY","INCLUDED_COACH_ROLES","VOTER_ENTITY","MATCHUP_SEASON","EFFECTIVE_TIME_SOURCE","MEDIA_ONLY"};
        auto get=[&](const auto& fields,const char* name)->std::string {const auto i=fields.find(name); if(i==fields.end()||i->second.value.empty()) throw BuildException(std::string("Missing season-event field ")+name,file,line); return i->second.value;};
        for(const auto& t:readTaggedFile(file))
        {
            if(t.tag=="SEASON_EVENTS_BEGIN"&&!begun&&!closed&&t.value.empty()){begun=true;continue;}
            if(t.tag=="SEASON_EVENTS_END"&&begun&&!closed&&!inRecord&&t.value.empty()){closed=true;continue;}
            if(!begun||closed)throw BuildException("Unexpected season-event envelope",file,t.line);
            if(t.tag=="EVENT_BEGIN"&&!inRecord&&t.value.empty())
            {
                if(get(header,"FORMAT_VERSION")!="0.1"||get(header,"DATA_VERSION")!="2"||get(header,"GENERATED_LEAGUE_DATE_POLICY")!="HISTORICAL_REFERENCE_ONLY")
                    throw BuildException("Unsupported season-events source contract",file,t.line);
                for(const auto* required:{"LEAGUE","SEASON","TIME_ZONE","PROVENANCE"})(void)get(header,required);
                const auto sourceYear=parseInt(get(header,"SEASON"),file,t.line,"SEASON");
                if(sourceYear<1||sourceYear>9999)throw BuildException("Invalid source season year",file,t.line);
                inRecord=true;record.clear();line=t.line;continue;
            }
            if(t.tag=="EVENT_END"&&inRecord&&t.value.empty())
            {
                const auto key=get(record,"EVENT_KEY"),trigger=get(record,"TRIGGER"),action=get(record,"ACTION");
                const auto priority=parseInt(get(record,"PRIORITY"),file,line,"PRIORITY");
                std::string kind,local,zone,utc;
                if(trigger=="AT_DATETIME")
                {
                    kind="FIXED";local=get(record,"LOCAL_DATETIME");zone=get(header,"TIME_ZONE");utc=get(record,"SCHEDULED_UTC");
                    if(!S::utc(utc)||ClockTime::normalize(local,zone)!=utc)throw BuildException("Season-event local/UTC mismatch",file,line);
                }
                else if(trigger=="AT_TEAM_LOCAL_DATETIME")
                {
                    kind="TEAM_HOME_LOCAL";local=get(record,"LOCAL_DATETIME");
                    if(!S::utc(local+"Z")||record.count("SCHEDULED_UTC")||get(record,"TIME_BASIS")!="TEAM_HOME_CITY_LOCAL"||get(record,"EXPAND_PER")!="ACTIVE_TEAM_SEASON"||get(record,"TIME_ZONE_SOURCE")!="TEAM_HOME_CITY")
                        throw BuildException("Team-local event requires home-city expansion without a shared UTC",file,line);
                }
                else if(trigger=="AFTER_MATCHUP_FINALIZED")
                {
                    kind="AFTER_CHAMPIONSHIP_FINAL";
                    if(record.count("LOCAL_DATETIME")||record.count("SCHEDULED_UTC")||get(record,"MATCHUP_ROLE")!="SUPER_BOWL"||get(record,"EFFECTIVE_TIME_SOURCE")!="MATCHUP_HISTORICAL_COMPLETED_UTC")
                        throw BuildException("Unsupported finalization template",file,line);
                }
                else throw BuildException("Unsupported season-event trigger",file,line);
                std::string raw;for(std::size_t i=line-1;i<t.line;++i)raw+=sourceLines.at(i)+"\n";
                S::run(c.db,"INSERT INTO CoreSeasonEventDefinitions VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                    {key,get(header,"LEAGUE")+"_"+get(header,"SEASON"),get(record,"EVENT_TYPE"),kind,
                     local.empty()?S::Value(nullptr):S::Value(local),zone.empty()?S::Value(nullptr):S::Value(zone),utc.empty()?S::Value(nullptr):S::Value(utc),std::int64_t(priority),action,std::int64_t(1),
                     kind=="TEAM_HOME_LOCAL"?std::string("EACH_ACTIVE_TEAM"):std::string("LEAGUE"),record.count("VOTER_ENTITY")?S::Value("SOURCE_PERSON_ELIGIBILITY_V1"):S::Value(nullptr),std::int64_t(record.count("MEDIA_ONLY")&&get(record,"MEDIA_ONLY")=="TRUE"),source,std::int64_t(line),raw});
                if(record.count("REQUIRES_EVENT_KEY"))dependencies.emplace_back(key,get(record,"REQUIRES_EVENT_KEY"));
                ++count;c.inserted();inRecord=false;continue;
            }
            auto& fields=inRecord?record:header;const auto& keys=inRecord?recordKeys:headerKeys;
            if(!keys.count(t.tag)||t.value.empty()||!fields.emplace(t.tag,t).second||(!inRecord&&count))
                throw BuildException("Unknown, duplicate or misplaced season-event field: "+t.tag,file,t.line);
        }
        if(!closed||inRecord||!count)throw BuildException("Incomplete/empty season-events source",file,line);
        for(const auto& d:dependencies)S::run(c.db,"INSERT INTO CoreSeasonEventDependencies VALUES(?,?)",{d.first,d.second});
        if(S::scalar(c.db,"WITH RECURSIVE reach(a,b) AS (SELECT EventKey,RequiresEventKey FROM CoreSeasonEventDependencies UNION SELECT r.a,d.RequiresEventKey FROM reach r JOIN CoreSeasonEventDependencies d ON d.EventKey=r.b) SELECT count(*) FROM reach WHERE a=b"))throw BuildException("Season-event dependencies contain a cycle",file);
        S::run(c.db,"UPDATE SourceFiles SET RecordsImported=? WHERE SourceFileID=?",{std::int64_t(count),source});
    }
    insertMetadata(c.db,c,"ClockSchedulerStorageVersion","1");
    insertMetadata(c.db,c,"ClockConfigurationProvenance","EXPLICIT_BUILD_INPUT_V1");
    insertMetadata(c.db,c,"ClockTimeConversionPolicy",ClockTime::Policy);
    if(request.clockProfiles.empty())c.diagnostic(DatabaseDiagnosticSeverity::Warning,"MISSING_CLOCK_PROFILE: game readiness requires explicit ground rules; no quarter duration was assumed.");
}
