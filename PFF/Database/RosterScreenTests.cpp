// Isolated integration checks. All generated files are beneath bin/RosterTests.
#include "Database/CoreManifest.h"
#include "Database/RosterPresentation.h"
#include "GCE/Database/WorldDatabase.h"
#include "GCE/Database/ClockTime.h"
#include "GCE/Database/TeamDataSql.h"
#include "Screens/RosterScreen.h"
#include "Screens/TeamScreen.h"
#include "Rendering/Surface.h"
#include "Rendering/Objects/TextObjectFactory.h"
#include "Input/Event.h"
#include "Input/CommandMap.h"
#include "App/ScreenManager.h"
#include "Styles/PFF_Theme.h"
#include <chrono>
#include <fstream>
#include <iostream>
namespace fs=std::filesystem;
namespace S=TeamDataSql;
namespace
{
int checks=0;
void check(bool yes,const std::string& why){if(!yes)throw std::runtime_error(why);++checks;}
struct Db
{
    sqlite3* p=nullptr;
    explicit Db(const fs::path& path){check(sqlite3_open_v2(path.u8string().c_str(),&p,SQLITE_OPEN_READWRITE,nullptr)==SQLITE_OK,"Open fixture");S::exec(p,"PRAGMA foreign_keys=ON");}
    ~Db(){sqlite3_close(p);}
};
void write(const fs::path& path,const std::string& bytes){std::ofstream out(path,std::ios::binary);out<<bytes;check(bool(out),"Write fixture");}
std::string render(Screen& screen,Surface& surface)
{
    screen.draw(surface);std::string text;
    for(int y=0;y<surface.buffer().getHeight();++y){for(int x=0;x<surface.buffer().getWidth();++x){auto c=surface.buffer().getDisplayGlyph(x,y);text+=c<128?static_cast<char>(c):'?';}text+='\n';}
    return text;
}
Point find(Surface& surface,const std::string& text,bool controlsOnly=true)
{
    const int left=(!controlsOnly||text=="POS"||text=="Return")?0:std::max(0,surface.buffer().getWidth()-34);
    for(int y=0;y<surface.buffer().getHeight();++y)
    {std::string row;for(int x=0;x<surface.buffer().getWidth();++x){auto c=surface.buffer().getDisplayGlyph(x,y);row+=c<128?static_cast<char>(c):'?';}auto p=row.find(text,left);if(p!=std::string::npos)return {static_cast<int>(p),y};}
    throw std::runtime_error("Missing rendered text: "+text);
}
void click(Screen& screen,Point p)
{
    for(auto action:{Input::MouseAction::Pressed,Input::MouseAction::Released})
    {Input::MouseEvent m;m.position=p;m.button=Input::MouseButton::Left;m.action=action;screen.handleEvent(Input::Event::mouse(m));}
}
}
int main()
{
try
{
    for(const auto* position:{"OG","OT","OC","G-T","G-C","C-G","C-T","OG-OT","OG-OC"})
    {
        WorldRosterPlayer lineman;lineman.position=position;lineman.depth=3;
        check(PffRoster::matches(lineman,"OL")&&PffRoster::matches(lineman,"Offense")&&
            !PffRoster::matches(lineman,"Defense")&&lineman.position==position&&lineman.depth==3,
            std::string("Roster recognizes source lineman position without renaming: ")+position);
    }
    check(PffRoster::group("CB")=="DB"&&PffRoster::group("G-CB")!="OL"&&
        PffRoster::group("OG-")!="OL","OL aliases do not absorb defensive or incomplete position codes");
    const auto root=fs::absolute(fs::path("bin/RosterTests")/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    const auto sources=root/"Source";
    for(const auto& package:std::vector<std::pair<fs::path,std::string>>{
        {"RuleSets/FirstEdition/FirstEdition.pffruleset","RULESET"},{"Seasons/1989/1989Season.pffseason","SEASON"}})
    {
        const auto original=fs::absolute(fs::path("PFF/Data/Source")/package.first);
        const auto manifest=CoreManifest::read(original,package.second);
        const auto folder=sources/package.first.parent_path();fs::create_directories(folder);
        fs::copy_file(original,sources/package.first);
        for(const auto& file:manifest.files)fs::copy_file(file,folder/file.filename());
    }
    const auto events=sources/"Seasons/1989/season_events.pffdata";
    if(!fs::exists(events))fs::copy_file("PFF/Data/Source/Seasons/1989/season_events.pffdata",events);
    const auto seasonManifest=sources/"Seasons/1989/1989Season.pffseason";
    auto manifestBytes=CoreManifest::bytes(seasonManifest);
    if(manifestBytes.find("season_events.pffdata")==std::string::npos)
    {manifestBytes.insert(manifestBytes.find("[FILES_END]"),"[FILE] season_events.pffdata\n");write(seasonManifest,manifestBytes);}
    const auto playerFile=sources/"Seasons/1989/players.pffdata";
    const auto originalPlayers=CoreManifest::bytes(playerFile);
    auto bytes=originalPlayers;
    bytes.insert(bytes.find("[PLAYER_BEGIN]")+14,"\n[YEARS_IN_LEAGUE]9\n[DEPTH_SLOT]3\n");write(playerFile,bytes);
    CoreDatabaseBuildRequest core;core.sourceRoot=sources;core.rulesetManifest=sources/"RuleSets/FirstEdition/FirstEdition.pffruleset";
    core.seasonManifest=seasonManifest;core.outputDatabase=root/"Core.db";
    core.clockProfiles.push_back({"fixture","UNSUPPORTED","FIXTURE_TIMING_V1","fixture-policy",1,600,1});
    const auto evidence=sources/"home-city-evidence.pffdata";write(evidence,"Home-city zone: America/New_York\n");
    std::string firstTeam;
    {std::ifstream in(sources/"Seasons/1989/league_structure.pffdata");std::string line;while(std::getline(in,line))if(line.find("[TEAM_SEASON_KEY]")!=std::string::npos){firstTeam=CoreManifest::trim(line.substr(line.find(']')+1));break;}}
    core.teamHomeTimeZones.push_back({firstTeam,"America/New_York",ClockTime::Policy,evidence,1});
    const auto built=createCoreDatabaseFromSource(core);
    if(!built.success)for(const auto& e:built.diagnostics)std::cerr<<e.message<<'\n';
    check(built.success,"Build roster Core");
    {
        Db db(core.outputDatabase);
        check(S::scalar(db.p,"SELECT count(*) FROM CoreRosterData")==1008,"All experience rows compiled");
        check(S::scalar(db.p,"SELECT YearsInLeague FROM CoreRosterData WHERE PlayerID=1")==9,"Explicit experience wins");
        check(S::scalar(db.p,"SELECT count(*) FROM CoreRosterData d JOIN Players p USING(PlayerID) WHERE d.ExperienceOrigin='AGE_MINUS_22' AND d.YearsInLeague<>max(1,p.Age-22)")==0,"Approved age fallback");
        check(S::scalar(db.p,"SELECT InitialDepth FROM CoreRosterData WHERE PlayerID=1")==3,"Authored depth retained");
    }
    WorldDatabaseCreateRequest request;request.coreDatabase=core.outputDatabase;request.outputDatabase=root/"World/World.db";request.worldName="Roster fixture";request.season=1989;
    const auto made=createWorldDatabase(request);
    if(!made.success)for(const auto& e:made.diagnostics)std::cerr<<e.message<<'\n';
    check(made.success,"Create roster World");
    auto loaded=loadWorldDatabase(request.outputDatabase);check(loaded.success,loaded.errorMessage);
    std::int64_t team=0,season=0,player=0;
    {
        Db db(request.outputDatabase);
        team=S::scalar(db.p,"SELECT min(TeamSeasonID) FROM TeamSeasons");season=S::scalar(db.p,"SELECT min(CompetitionSeasonID) FROM CompetitionSeasons");
        check(S::scalar(db.p,"SELECT count(*) FROM WorldRosterDepth")==1008,"Depth initialized for every player");
        check(S::scalar(db.p,"SELECT count(*) FROM pragma_foreign_key_check")==0,"Roster foreign keys");
        player=S::scalar(db.p,"SELECT min(WorldPlayerSeasonID) FROM WorldPlayerSeasons");
        team=S::scalar(db.p,"SELECT TeamSeasonID FROM WorldRosterDepth WHERE WorldPlayerSeasonID=?",{player});
        check(S::scalar(db.p,"SELECT DepthSlot FROM WorldRosterDepth WHERE WorldPlayerSeasonID=?",{player})==3,"Explicit QB3 preserved");
    }
    auto roster=loadWorldRoster(request.outputDatabase,season,team);
    check(roster.success,roster.errorMessage);check(roster.players.size()==36,"Complete team");
    check(!loadWorldRoster(request.outputDatabase,season,9999999).success,"Wrong team fails");
    auto first=std::find_if(roster.players.begin(),roster.players.end(),[&](const auto& p){return p.playerSeasonId==player;});check(first!=roster.players.end(),"Selected fixture player");
    check(first->yearsInLeague==9&&PffRoster::status(*first)=="OK","Explicit experience and healthy projection");
    const auto kicker=std::find_if(roster.players.begin(),roster.players.end(),[](const auto& value){return value.cardType=="KICKER";});
    check(kicker!=roster.players.end(),"Kicker card present");
    const auto kickColumns=PffRoster::columns(*kicker);
    check(kickColumns.size()==14&&kickColumns[8].key=="FG_1_TO_12"&&kickColumns[12].key=="FG_38_TO_42"&&kickColumns.back().key=="BLOCKED","Field goal bands retain source order and distinct headings");
    auto p=*first;p.healthStatus="QUESTIONABLE";
    const auto col=PffRoster::columns(p).front();
    auto base=std::find_if(p.ratings.begin(),p.ratings.end(),[&](const auto& r){return r.key==col.key;})->baseline;
    check(PffRoster::rating(p,col).first==PffRoster::number(base-2),"Q reduces performance");
    p.modifiers.push_back({col.key,"INJURY",-2});check(PffRoster::rating(p,col).first==PffRoster::number(base-2),"Stored injury not double-counted");
    p.ratings.push_back({"ENDURANCE",5});check(PffRoster::rating(p,{"ENDURANCE","END","Endurance"}).first=="5","Endurance exempt");
    p.modifiers.push_back({"ENDURANCE","INJURY",-2});
    check(PffRoster::rating(p,{"ENDURANCE","END","Endurance"}).first=="5","Stored P/Q effect also exempts endurance");
    check(PffRoster::rating(p,{"ENDURANCE","END","Endurance"}).second.empty(),"Exempt endurance has no modifier tooltip");
    p.healthStatus="OUT";p.gamesRemaining=2;check(PffRoster::status(p)=="O2","Out games display");
    p.rosterStatus="IR";check(PffRoster::status(p)=="IR"&&PffRoster::matches(p,"Injured"),"IR visible injured");
    p=*first;p.position="WR";check(PffRoster::matches(p,"SP")&&p.depth==first->depth,"SP preserves identity");
    LoadedWorldContext context;context.loaded=true;context.databasePath=request.outputDatabase;context.worldKey=loaded.world.worldKey;
    context.activeCompetitionSeasonId=season;context.worldName="Roster fixture";context.currentTimeUtc=loaded.world.currentTimeUtc;
    Assets::AssetLibrary assets;bool back=false;
    // Validate all teams against their compiled source cards, including the
    // actual San Francisco OG/OT/OC regression. Do not alter genuine saves.
    {
        Db coreDb(core.outputDatabase),worldDb(request.outputDatabase);
        S::Query teams(worldDb.p,"SELECT TeamSeasonID,OriginCoreTeamSeasonKey FROM TeamSeasons ORDER BY TeamSeasonID");
        int teamsChecked=0;
        while(teams.row())
        {
            const auto id=teams.integer();const auto key=teams.text(1);
            auto full=loadWorldRoster(request.outputDatabase,season,id);
            check(full.success&&full.players.size()==36,"Complete 36-player World roster for "+key);
            S::Query sourceLinemen(coreDb.p,"SELECT p.Position,p.LastName,p.FirstName FROM Players p "
                "JOIN PlayerRatingProfiles pr USING(PlayerID) JOIN TeamSeasons t ON t.TeamSeasonID=p.TeamSeasonID "
                "WHERE t.TeamSeasonKey=? AND pr.CardType='OFFENSIVE_LINEMAN' AND p.Position<>'CB'",{key});
            // Existing source anomaly: J.C. Pearson is labeled CB but has an
            // OL card profile. Preserve his actual DB position, not that error.
            std::vector<std::vector<std::string>> expected;
            while(sourceLinemen.row())expected.push_back({sourceLinemen.text(),sourceLinemen.text(1),sourceLinemen.text(2)});
            check(std::count_if(full.players.begin(),full.players.end(),[](const auto& value){return PffRoster::matches(value,"OL");})==expected.size(),
                "OL filter includes every source lineman for "+key);
            RosterScreen teamRoster(assets,[](std::string_view){return true;},[&]{return context;},context,id);
            teamRoster.onEnter();Surface teamSurface(180,80);render(teamRoster,teamSurface);
            click(teamRoster,find(teamSurface,"Offense"));auto offense=render(teamRoster,teamSurface);
            click(teamRoster,find(teamSurface,"OL"));const auto offensiveLine=render(teamRoster,teamSurface);
            bool allVisible=true,unchanged=true;
            for(const auto& sourcePlayer:expected)
            {
                const auto name=sourcePlayer[1]+", "+sourcePlayer[2];
                allVisible=allVisible&&offense.find(name)!=std::string::npos&&offensiveLine.find(name)!=std::string::npos;
                const auto found=std::find_if(full.players.begin(),full.players.end(),[&](const auto& value){return value.lastName==sourcePlayer[1]&&value.firstName==sourcePlayer[2];});
                unchanged=unchanged&&found!=full.players.end()&&found->position==sourcePlayer[0]&&
                    offensiveLine.find(found->position+std::to_string(found->depth))!=std::string::npos;
            }
            check(allVisible,"Offense and OL screens render all source linemen for "+key);
            check(unchanged,"OL display preserves imported position and persisted depth for "+key);
            if(key=="PFF_1989_SAN_FRANCISCO_49ERS")
                check(expected.size()==7&&offensiveLine.find("Sapolu, Jesse")!=std::string::npos&&
                    offensiveLine.find("McIntyre, Guy")!=std::string::npos&&offensiveLine.find("Barton, Harris")!=std::string::npos,
                    "San Francisco renders its seven OG/OT/OC linemen");
            ++teamsChecked;
        }
        check(teamsChecked==28,"Validated offensive line display across all 28 teams");
    }
    RosterScreen screen(assets,[&](std::string_view c){back=c=="nav:back";return true;},[&]{return context;},context,team);
    const auto worldBeforeUi=CoreManifest::bytes(request.outputDatabase);
    screen.onEnter();Surface surface(180,70);
    auto text=render(screen,surface);check(text.find("Roster")!=std::string::npos&&text.find("QB3")!=std::string::npos,"Roster rendered");
    write(root/"Roster-All.txt",text);
    // Exercise the screen's real focus/activation and rendered player highlight.
    {
        RosterScreen keyboard(assets,[](std::string_view){return true;},[&]{return context;},context,team);
        keyboard.onEnter();Surface keys(120,30);render(keyboard,keys);
        auto command=[&](Input::CommandCode code){
            Input::KeyEvent raw;raw.pressed=true;
            if(code==Input::CommandCode::NextFocus)raw.code=Input::KeyCode::Tab;
            else if(code==Input::CommandCode::MoveUp)raw.code=Input::KeyCode::Up;
            else if(code==Input::CommandCode::MoveDown)raw.code=Input::KeyCode::Down;
            if(raw.code!=Input::KeyCode::Unknown)keyboard.handleEvent(Input::Event::key(raw));
            keyboard.handleEvent(Input::Event::command(Input::Command{code}));render(keyboard,keys);};
        auto highlighted=[&](){std::string result;for(int y=0;y<keys.buffer().getHeight();++y)
            for(int x=0;x<80;++x)if(keys.buffer().getDisplayStyle(x,y)==PFF_Colors::ScrollBoxRowHighlight)
            {for(;x<80&&keys.buffer().getDisplayStyle(x,y)==PFF_Colors::ScrollBoxRowHighlight;++x)
                {const auto c=keys.buffer().getDisplayGlyph(x,y);result+=c<128?static_cast<char>(c):'?';}return result;}return result;};
        command(Input::CommandCode::NextFocus);command(Input::CommandCode::Confirm);
        check(render(keyboard,keys).find(" - Offense")!=std::string::npos,"Tab from All focuses and activates Offense");
        for(const auto* label:{"Defense","Injured","QB","RB","WR","TE","OL","SP","Return"})
        {
            command(Input::CommandCode::NextFocus);const auto p=find(keys,label);
            check(keys.buffer().getDisplayStyle(p.x,p.y)==PFF_Colors::ButtonHoverColor,
                std::string("Tab visits and reveals ")+label);
        }
        command(Input::CommandCode::NextFocus);
        const auto initial=highlighted();check(!initial.empty()&&initial.find("QB1")!=std::string::npos,"Return then Tab enters list and highlights first player");
        command(Input::CommandCode::MoveUp);check(highlighted()==initial,"Up at first player retains selection");
        std::vector<std::string> visited{initial};
        const auto offenseCount=std::count_if(roster.players.begin(),roster.players.end(),[](const auto& value){return PffRoster::matches(value,"Offense");});
        for(int i=1;i<offenseCount;++i){command(Input::CommandCode::MoveDown);const auto row=highlighted();
            check(!row.empty()&&std::find(visited.begin(),visited.end(),row)==visited.end(),"Down selects next unique player, skips headings and scrolls into view");visited.push_back(row);}
        command(Input::CommandCode::MoveDown);check(highlighted()==visited.back(),"Down at last player retains selection");
        for(int i=static_cast<int>(visited.size())-2;i>=0;--i){command(Input::CommandCode::MoveUp);check(highlighted()==visited[i],"Up reverses player order and scrolls into view");}
        command(Input::CommandCode::NextFocus);command(Input::CommandCode::Confirm);
        check(render(keyboard,keys).find(" - All")!=std::string::npos,"Tab from list returns to top All button");
        // All has only primary buttons; hidden position filters must be skipped.
        for(int i=0;i<4;++i)command(Input::CommandCode::NextFocus);
        const auto returnPoint=find(keys,"Return");
        check(keys.buffer().getDisplayStyle(returnPoint.x,returnPoint.y)==PFF_Colors::ButtonHoverColor,"All Tab order skips hidden position buttons");
        command(Input::CommandCode::NextFocus);check(!highlighted().empty(),"All list accepts keyboard focus");
        // Match ApplicationHost: raw key first, then its mapped command.
        Input::KeyEvent key;key.code=Input::KeyCode::Tab;key.pressed=true;
        keyboard.handleEvent(Input::Event::key(key));
        const auto mapped=Input::CommandMap::createDefault().map(key);
        check(mapped.has_value(),"Default command map translates Tab");
        keyboard.handleEvent(Input::Event::command(*mapped));command(Input::CommandCode::Confirm);
        check(render(keyboard,keys).find(" - All")!=std::string::npos,"Host raw key plus Tab command wraps focus exactly once to All");
        command(Input::CommandCode::PreviousFocus);check(!highlighted().empty(),"Shift Tab reverses from All into roster");
        command(Input::CommandCode::NextFocus);
        for(int i=0;i<3;++i)command(Input::CommandCode::NextFocus);
        command(Input::CommandCode::Confirm);
        check(render(keyboard,keys).find("No players match")!=std::string::npos,"Keyboard activates empty Injured filter");
        command(Input::CommandCode::NextFocus);command(Input::CommandCode::NextFocus);
        command(Input::CommandCode::MoveDown);command(Input::CommandCode::MoveUp);
        check(highlighted().empty(),"Empty roster does not select a heading or message");
        command(Input::CommandCode::NextFocus);command(Input::CommandCode::Confirm);
        check(render(keyboard,keys).find(" - All")!=std::string::npos,"Tab exits an empty roster to All");
    }
    auto headingCount=[](const std::string& rendered){std::size_t count=0;std::istringstream lines(rendered);std::string line;while(std::getline(lines,line)){const auto pos=line.find("POS");if(pos==std::string::npos)continue;std::istringstream words(line.substr(pos));std::string first,second;words>>first>>second;if(first=="POS"&&second=="St")++count;}return count;};
    auto normalized=[](const std::string& rendered){std::istringstream stream(rendered);std::string result,word;while(stream>>word){if(!result.empty())result+=' ';result+=word;}return result;};
    check(headingCount(text)>1,"All retains profile subheadings");
    click(screen,find(surface,"Offense"));text=render(screen,surface);check(text.find("SP")!=std::string::npos,"Offense subfilters appear");
    check(headingCount(text)>1,"Offense retains profile subheadings");
    for(const auto* label:{"QB","RB","WR","TE","OL","SP"})
    {
        click(screen,find(surface,label));text=render(screen,surface);
        check(headingCount(text)==1&&text.find(" / ")==std::string::npos,std::string(label)+" has one heading and no profile subheading");
        if(std::string(label)=="QB")
        {
            check(normalized(text).find("PQ PS PM PL")!=std::string::npos,"QB heading contains actual rating labels");
            const auto start=find(surface,"POS");
            const int contentWidth=surface.buffer().getWidth()-std::clamp(surface.buffer().getWidth()/3,28,34);
            const auto hint=find(surface,"Arrows/Page keys:",false);
            const std::string hintText="Arrows/Page keys: scroll; hover headings or values for details.";
            check(hint.x==(contentWidth-static_cast<int>(hintText.size()))/2&&hint.y==surface.buffer().getHeight()-6,"Scroll guidance is centered on its original content row");
            int end=start.x;
            while(end<contentWidth&&surface.buffer().getDisplayStyle(end,start.y)==PFF_Colors::ScrollBoxHeading)++end;
            check(start.x>0&&end<contentWidth&&std::abs(start.x-(contentWidth-end))<=1,"Roster list uses natural width and is centered in content pane");
            check(start.y==14,"Resized list retains its top alignment");
            const auto count=std::count_if(roster.players.begin(),roster.players.end(),[](const auto& value){return PffRoster::matches(value,"QB");});
            const int bottom=start.y+1+static_cast<int>(count);
            check(surface.buffer().getDisplayStyle(start.x,bottom)==PFF_Colors::FormBorderColor&&
                surface.buffer().getDisplayStyle(start.x,bottom+1)==PFF_Colors::MainBackgroundColor,"Short roster panel ends immediately below its last row");
            const auto borderGlyphs=ObjectFactory::singleLineBorder();
            check(surface.buffer().getDisplayGlyph(start.x-1,start.y)==borderGlyphs.vertical&&
                surface.buffer().getDisplayGlyph(end,start.y)==borderGlyphs.vertical&&
                surface.buffer().getDisplayGlyph(start.x,bottom)==borderGlyphs.horizontal,"Single-line panel surrounds the roster list");
            std::string panelTitle;
            for(int x=start.x-1;x<=end;++x)panelTitle+=static_cast<char>(surface.buffer().getDisplayGlyph(x,start.y-1));
            check(panelTitle.find(" Roster ")!=std::string::npos,"Roster title appears in the panel's upper border");
            const auto playerName=find(surface,first->lastName+", "+first->firstName,false);
            {Input::MouseEvent m;m.action=Input::MouseAction::Moved;m.position=playerName;screen.handleEvent(Input::Event::mouse(m));}
            check(render(screen,surface)==text,"Player identity data has no tooltip");
            const auto quickHeading=find(surface,"PQ",false);
            {Input::MouseEvent m;m.action=Input::MouseAction::Moved;m.position={quickHeading.x,playerName.y};screen.handleEvent(Input::Event::mouse(m));}
            check(render(screen,surface)==text,"Unmodified rating has no tooltip");
        }
    }
    click(screen,find(surface,"Defense"));text=render(screen,surface);
    check(headingCount(text)>1,"Defense retains profile subheadings");
    for(const auto* label:{"DL","LB","DB"})
    {
        click(screen,find(surface,label));text=render(screen,surface);
        check(headingCount(text)==1&&text.find(" / ")==std::string::npos,std::string(label)+" has one heading and no profile subheading");
    }
    click(screen,find(surface,"Offense"));render(screen,surface);
    click(screen,find(surface,"SP"));text=render(screen,surface);check(text.find("QB3")==std::string::npos,"SP excludes QB");
    {Input::MouseEvent m;m.action=Input::MouseAction::Moved;m.position=find(surface,"BSP",false);screen.handleEvent(Input::Event::mouse(m));}
    text=render(screen,surface);
    check(text.find("Breakaway speed")!=std::string::npos&&text.find("[BREAK_SPEED]")==std::string::npos,"Position heading tooltip uses readable name without bracketed key");
    click(screen,find(surface,"Offense"));render(screen,surface);
    {Input::MouseEvent m;m.action=Input::MouseAction::Moved;m.position=find(surface,"BSP",false);screen.handleEvent(Input::Event::mouse(m));}
    text=render(screen,surface);
    check(text.find("Breakaway speed")!=std::string::npos&&text.find("[BREAK_SPEED]")==std::string::npos,"Mixed-profile heading tooltip omits bracketed key");
    click(screen,find(surface,"Injured"));text=render(screen,surface);check(text.find("No players match")!=std::string::npos,"Empty injured view");
    click(screen,find(surface,"All"));text=render(screen,surface);check(text.find("QB3")!=std::string::npos,"All restores roster");
    // Hover the common heading and inspect the actual rendered tooltip.
    {Input::MouseEvent m;m.action=Input::MouseAction::Moved;m.position=find(surface,"POS");screen.handleEvent(Input::Event::mouse(m));}
    text=render(screen,surface);check(text.find("Position and depth slot")!=std::string::npos,"Heading tooltip");
    click(screen,find(surface,"Return"));check(back,"Return dispatches Back");
    check(CoreManifest::bytes(request.outputDatabase)==worldBeforeUi,"UI reads do not mutate World");
    // The actual navigation stack must reveal the same caller instance.
    struct Caller final:Screen {void draw(Surface&)override{}};
    ScreenManager stack;auto caller=std::make_unique<Caller>();const auto* identity=caller.get();stack.pushScreen(std::move(caller));
    stack.pushScreen(std::make_unique<RosterScreen>(assets,[&](std::string_view c){if(c=="nav:back")stack.popScreen();return true;},[&]{return context;},context,team));
    render(*stack.currentScreen(),surface);click(*stack.currentScreen(),find(surface,"Return"));
    check(stack.screenCount()==1&&stack.currentScreen()==identity,"Return reveals the existing caller");
    ScreenManager teamStack;
    std::function<bool(std::string_view)> teamDispatch=[&](std::string_view c){if(c=="nav:back")teamStack.popScreen();return true;};
    auto actualTeam=std::make_unique<TeamScreen>(assets,teamDispatch,[&]{return context;},context,team,
        std::function<void(const LoadedWorldContext&,std::int64_t)>{},
        std::function<void(const LoadedWorldContext&,std::int64_t)>{},
        std::function<void(const LoadedWorldContext&,std::int64_t)>{},
        [&](const LoadedWorldContext& selected,std::int64_t id){teamStack.pushScreen(std::make_unique<RosterScreen>(assets,teamDispatch,[&]{return context;},selected,id));});
    const auto* actualTeamIdentity=actualTeam.get();teamStack.pushScreen(std::move(actualTeam));
    render(*teamStack.currentScreen(),surface);click(*teamStack.currentScreen(),find(surface,"Roster"));
    check(teamStack.screenCount()==2&&dynamic_cast<RosterScreen*>(teamStack.currentScreen()),"Team Roster button opens screen");
    render(*teamStack.currentScreen(),surface);click(*teamStack.currentScreen(),find(surface,"Return"));
    check(teamStack.currentScreen()==actualTeamIdentity&&teamStack.screenCount()==1,"Roster returns to same TeamScreen");
    // Mixed initial ranks use source order around reserved explicit gaps.
    std::vector<std::string> mixedKeys;
    {
        Db db(core.outputDatabase);
        S::Query group(db.p,"SELECT TeamSeasonID,Position FROM Players GROUP BY TeamSeasonID,Position HAVING count(*)>=3 LIMIT 1");
        check(group.row(),"Source group for mixed-depth fixture");
        S::Query keys(db.p,"SELECT p.PlayerID,p.SourceLocalKey FROM Players p JOIN CoreRosterData d USING(PlayerID) WHERE p.TeamSeasonID=? AND p.Position=? ORDER BY d.SourceOrder",{group.integer(),group.text(1)});
        int index=0;
        while(keys.row())
        {
            mixedKeys.push_back(keys.text(1));
            if(index==1||index==2)S::run(db.p,"UPDATE CoreRosterData SET InitialDepth=? WHERE PlayerID=?",{static_cast<std::int64_t>(index==1?1:5),keys.integer()});
            else S::run(db.p,"UPDATE CoreRosterData SET InitialDepth=NULL WHERE PlayerID=?",{keys.integer()});
            ++index;
        }
    }
    auto mixedRequest=request;mixedRequest.outputDatabase=root/"Mixed/World.db";
    const auto mixedMade=createWorldDatabase(mixedRequest);check(mixedMade.success,"Mixed-slot World creation");
    {
        Db db(mixedRequest.outputDatabase);
        for(const auto index:{0,1,2})
        {
            const auto depth=S::scalar(db.p,"SELECT d.DepthSlot FROM WorldRosterDepth d JOIN WorldPlayerSeasons p USING(WorldPlayerSeasonID) WHERE p.CorePlayerKey=?",{mixedKeys[index]});
            check(depth==(index==0?2:index==1?1:5),"Reserved slots and source-order gap filling");
        }
        bool rejected=false;
        try {S::exec(db.p,"UPDATE WorldRosterDepth SET TeamSeasonID=999999 WHERE RosterAssignmentID=(SELECT min(RosterAssignmentID) FROM WorldRosterDepth)");}
        catch(const std::exception&){rejected=true;}
        check(rejected,"Cross-team depth change rejected");
    }
    {Db db(core.outputDatabase);S::run(db.p,"UPDATE CoreRosterData SET InitialDepth=1 WHERE PlayerID=(SELECT PlayerID FROM Players WHERE SourceLocalKey=?)",{mixedKeys[0]});}
    mixedRequest.outputDatabase=root/"Duplicate/World.db";
    check(!createWorldDatabase(mixedRequest).success&&!fs::exists(mixedRequest.outputDatabase),"Duplicate explicit slots reject World publication");
    check(loadWorldRoster(request.outputDatabase,season,team).success,"Global Core edits cannot change preserved roster snapshot");
    // Current availability is read from World across every unit.
    std::vector<std::int64_t> injured;
    {
        Db db(request.outputDatabase);
        for(const auto& wanted:std::vector<std::pair<std::string,std::string>>{{"QB","PROBABLE"},{"WR","QUESTIONABLE"},{"DL","OUT"},{"K","HEALTHY"}})
        {
            const auto found=std::find_if(roster.players.begin(),roster.players.end(),[&](const auto& value){return PffRoster::group(value.position)==wanted.first;});
            check(found!=roster.players.end(),"Availability fixture position");injured.push_back(found->playerSeasonId);
            S::run(db.p,"UPDATE PlayerState SET HealthStatus=? WHERE WorldPlayerSeasonID=?",{wanted.second,found->playerSeasonId});
            if(wanted.first=="DL")
            {
                S::run(db.p,"INSERT INTO PlayerInjuries(WorldPlayerSeasonID,GamesRemaining,Status) VALUES(?,2,'ACTIVE')",{found->playerSeasonId});
                S::run(db.p,"UPDATE PlayerState SET CurrentInjuryID=? WHERE WorldPlayerSeasonID=?",{S::inserted(db.p),found->playerSeasonId});
            }
            if(wanted.first=="K")S::run(db.p,"UPDATE RosterAssignments SET RosterStatus='IR' WHERE WorldPlayerSeasonID=? AND EndDateUtc IS NULL",{found->playerSeasonId});
        }
    }
    ++context.dataRevision;surface.resize(180,70);render(screen,surface);
    click(screen,find(surface,"Injured"));text=render(screen,surface);
    check(text.find("No players match")==std::string::npos&&text.find("O2")!=std::string::npos&&text.find("IR")!=std::string::npos,"Injured includes current Out and IR");
    check(headingCount(text)>1,"Injured retains profile subheadings");
    const auto current=loadWorldRoster(request.outputDatabase,season,team);check(current.success,current.errorMessage);
    check(std::count_if(current.players.begin(),current.players.end(),[](const auto& value){return PffRoster::matches(value,"Injured");})==4,"P Q Out IR across units");
    write(root/"Roster-Injured.txt",text);
    click(screen,find(surface,"Offense"));render(screen,surface);
    click(screen,find(surface,"QB"));render(screen,surface);
    const auto probable=std::find_if(current.players.begin(),current.players.end(),[](const auto& value){return value.healthStatus=="PROBABLE";});
    check(probable!=current.players.end(),"Modified rating fixture player");
    const auto probableName=find(surface,probable->lastName+", "+probable->firstName,false);
    const auto quickHeading=find(surface,"PQ",false);
    {Input::MouseEvent m;m.action=Input::MouseAction::Moved;m.position={quickHeading.x,probableName.y};screen.handleEvent(Input::Event::mouse(m));}
    text=render(screen,surface);
    const auto originalQuick=std::find_if(probable->ratings.begin(),probable->ratings.end(),[](const auto& value){return value.key=="PASS_QUICK";});
    check(originalQuick!=probable->ratings.end(),"Original quick-pass rating available");
    check(text.find("Original: "+PffRoster::number(originalQuick->baseline))!=std::string::npos&&
        text.find("Current:")==std::string::npos&&text.find("[PASS_QUICK]")==std::string::npos,"Modified rating tooltip shows only Original value");
    for(const auto size:std::vector<Size>{{120,40},{90,30},{60,25},{20,15}}){surface.resize(size.width,size.height);render(screen,surface);}
    {
        Db db(request.outputDatabase);S::run(db.p,"DELETE FROM PlayerState WHERE WorldPlayerSeasonID=?",{player});
    }
    check(!loadWorldRoster(request.outputDatabase,season,team).success,"Missing availability is an error");
    // No capability: old leagues are read-only and get explicit guidance.
    {Db db(request.outputDatabase);S::exec(db.p,"DELETE FROM WorldFeatureVersions WHERE FeatureKey='ROSTER_DATA'");}
    const auto old=loadWorldRoster(request.outputDatabase,season,team);check(!old.success&&old.errorMessage.find("new Core and league")!=std::string::npos,"Old capability guidance");
    // Core failure must never publish a partially compiled output.
    bytes=originalPlayers;bytes.insert(bytes.find("[PLAYER_BEGIN]")+14,"\n[DEPTH_SLOT]0\n");write(playerFile,bytes);core.outputDatabase=root/"Invalid.db";
    check(!createCoreDatabaseFromSource(core).success&&!fs::exists(core.outputDatabase),"Invalid depth rejected atomically");
    std::cout<<"PASS "<<checks<<" checks; fixtures: "<<root.u8string()<<'\n';return 0;
}
catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
