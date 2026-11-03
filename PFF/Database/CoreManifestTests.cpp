// Standalone regression executable; deliberately excluded from the PFF app.
// Run from repository root. All writable fixtures live in bin/ManifestTests.
#include "CoreManifest.h"
#include "PFFDatabase.h"
#include <sqlite3.h>
#include <chrono>
#include <iostream>
#include <thread>
namespace fs = std::filesystem;
void check(bool passed, const std::string& message);
#ifdef CORE_MANIFEST_UI_TESTS
#include "Screens/CommissionersOfficeScreen.h"
#include "Rendering/Surface.h"
#include "Input/Event.h"
#include "Input/InputManager.h"
#include "Input/VtEditingKey.h"
#include "Styles/PFF_Theme.h"
#include "App/ScreenManager.h"
#include "Screens/LeagueScheduleScreen.h"

Input::Event normalizedKey(std::uint16_t vk, char32_t character)
{
    class Source final : public Input::IRawKeyboardSource
    {
    public:
        std::optional<Input::RawInputEvent> value;
        bool initialize() override { return true; }
        void shutdown() override {}
        std::optional<Input::RawInputEvent> pollRawInput() override { auto event=value; value.reset(); return event; }
    };
    auto source=std::make_unique<Source>();
    Input::RawKeyEvent raw; raw.virtualKey=vk; raw.character=character; raw.pressed=true;
    source->value=raw;
    Input::InputManager manager(std::move(source));
    check(manager.initialize(), "Test input source failed"); manager.poll();
    auto event=manager.popEvent(); check(event.has_value(), "Input normalization produced no event"); return *event;
}

void testScheduleCache(const fs::path& world,const WorldDatabaseLoadResult& loaded)
{
    LoadedWorldContext context;
    context.loaded=true;context.databasePath=world;context.worldKey=loaded.world.worldKey;
    context.worldName=loaded.world.worldName;context.currentTimeUtc=loaded.world.currentTimeUtc;
    check(!loaded.competitionSeasons.empty(),"Schedule cache fixture has no season");
    context.activeCompetitionSeasonId=loaded.competitionSeasons.front().competitionSeasonId;
    Assets::AssetLibrary assets;
    LeagueScheduleScreen screen(assets,[](std::string_view) {return true;},[&] {return context;});
    Surface surface(160,32);
    auto render=[&] {screen.draw(surface);return surface.buffer().renderToUtf8String();};
    auto command=[&](Input::CommandCode code) {
        screen.handleEvent(Input::Event::command(Input::Command{code}));return render();
    };
    auto click=[&](const std::string& label) {
        Point position{-1,-1};
        for(int y=0;y<surface.buffer().getHeight();++y)
        {
            std::string row;
            for(int x=0;x<surface.buffer().getWidth();++x)
                row+=static_cast<char>(surface.buffer().getDisplayGlyph(x,y));
            const auto x=row.find(label);
            if(x!=std::string::npos) position=Point{static_cast<int>(x),y};
        }
        check(position.x>=0,"Schedule cache missing button: "+label);
        for(auto action : {Input::MouseAction::Pressed,Input::MouseAction::Released})
        {
            Input::MouseEvent event;event.position=position;event.action=action;event.button=Input::MouseButton::Left;
            screen.handleEvent(Input::Event::mouse(event));
        }
        return render();
    };
    screen.onEnter();const auto first=render();const ScreenBuffer original=surface.buffer();
    check(render()==first,"Schedule cache changed text on repeated draw");
    for(int y=0;y<32;++y) for(int x=0;x<160;++x)
        check(original.getDisplayStyle(x,y)==surface.buffer().getDisplayStyle(x,y),
            "Schedule cache changed styles on repeated draw");
    const auto scrolled=command(Input::CommandCode::MoveEnd);
    check(scrolled!=first,"Schedule cache failed to invalidate for scrolling");
    check(command(Input::CommandCode::MoveHome)==first,"Schedule cache failed to restore scroll position");
    const auto weeks=loadWorldCompetitionSchedule(world,context.activeCompetitionSeasonId).regularSeasonWeeks;
    check(weeks.size()>1,"Schedule cache test requires multiple weeks");
    check(click("Look Ahead").find("WEEK "+std::to_string(weeks[1]))!=std::string::npos,
        "Schedule cache failed to invalidate for Look Ahead");
    check(click("Look Back").find("WEEK "+std::to_string(weeks[0]))!=std::string::npos,
        "Schedule cache failed to invalidate for Look Back");
    context.worldName="Cache test league";
    check(render().find(context.worldName)!=std::string::npos,"Schedule cache ignored league context change");
    context.currentTimeUtc="1990-02-01T17:00:00Z";
    check(render().find("February 1, 1990")!=std::string::npos,"Schedule cache ignored game clock change");
    surface.resize(120,48);
    check(render().find("Enlarge the window")!=std::string::npos,"Schedule cache ignored resize");
    context.clear();
    check(render().find("Load a league")!=std::string::npos,"Schedule cache retained an unloaded league");
    screen.onExit();screen.onEnter();
    check(render().find("Load a league")!=std::string::npos,"Schedule cache retained data across re-entry");
    std::cout<<"PASS: schedule cache text/styles, scroll, week navigation, league/clock changes, resize and re-entry.\n";
}

void testScreen(const fs::path& fixture)
{
    // Exercise real synchronous screen destruction through the Return button.
    // Earlier UI tests used a dispatcher stub that never popped a screen.
    for(bool keyboard : {false,true})
    {
        ScreenManager manager;
        Assets::AssetLibrary navigationAssets;
        bool returned=false;
        manager.pushScreen(std::make_unique<CommissionersOfficeScreen>(navigationAssets,
            [&](std::string_view command) {
                check(command=="nav:back","Return dispatched the wrong command");
                manager.popScreen();returned=true;return true;
            }, [] {return LoadedWorldContext{};}, [](const fs::path&) {return false;}));
        Surface navigationSurface(160,55);
        manager.drawCurrentScreen(navigationSurface);
        if(keyboard)
        {
            for(int tab=0;tab<4;++tab)
                manager.dispatchEvent(Input::Event::command(Input::Command{Input::CommandCode::NextFocus}));
            manager.dispatchEvent(Input::Event::command(Input::Command{Input::CommandCode::Confirm}));
        }
        else
        {
            Point returnPosition{-1,-1};
            for(int y=0;y<navigationSurface.buffer().getHeight();++y)
            {
                std::string row;
                for(int x=0;x<navigationSurface.buffer().getWidth();++x)
                    row+=static_cast<char>(navigationSurface.buffer().getDisplayGlyph(x,y));
                const auto label=row.find("Return");
                if(label!=std::string::npos) returnPosition=Point{static_cast<int>(label),y};
            }
            check(returnPosition.x>=0,"Return button was not rendered");
            for(auto action : {Input::MouseAction::Pressed,Input::MouseAction::Released})
            {
                Input::MouseEvent mouse;mouse.position=returnPosition;
                mouse.button=Input::MouseButton::Left;mouse.action=action;
                manager.dispatchEvent(Input::Event::mouse(mouse));
            }
        }
        check(returned && !manager.hasScreens(),"Return did not safely remove the Commissioner screen");
    }
    const auto previous = fs::current_path();
    const auto source = previous / "PFF/Data/Source";
    for (const auto& package : {
        CoreManifest::read(source / "RuleSets/FirstEdition/FirstEdition.pffruleset", "RULESET"),
        CoreManifest::read(source / "Seasons/1989/1989Season.pffseason", "SEASON")})
    {
        auto copy = [&](const fs::path& file) {
            const auto target = fixture / "Data/Source" / fs::relative(file, source);
            fs::create_directories(target.parent_path());
            fs::copy_file(file, target);
        };
        copy(package.path);
        for (const auto& file : package.files) copy(file);
    }
    fs::current_path(fixture);
    Assets::AssetLibrary assets;
    int dispatches = 0;
    CommissionersOfficeScreen screen(assets, [&](std::string_view) { ++dispatches; return true; },
        [] { return LoadedWorldContext{}; }, [](const fs::path& path) { return loadWorldDatabase(path).success; });
    Surface surface(160, 55);
    auto render = [&] {
        screen.draw(surface);
        std::string text;
        for (int y = 0; y < surface.buffer().getHeight(); ++y)
        {
            for (int x = 0; x < surface.buffer().getWidth(); ++x)
            {
                const auto glyph = surface.buffer().getDisplayGlyph(x, y);
                text += glyph < 128 ? static_cast<char>(glyph) : '?';
            }
            text += '\n';
        }
        return text;
    };
    auto command = [&](Input::CommandCode code) { screen.handleEvent(Input::Event::command(Input::Command{code})); };
    auto click = [&](int x,int y) {
        for(auto action:{Input::MouseAction::Pressed,Input::MouseAction::Released}) {
            Input::MouseEvent mouse; mouse.position=Point{x,y}; mouse.button=Input::MouseButton::Left; mouse.action=action;
            screen.handleEvent(Input::Event::mouse(mouse));
        }
    };
    render(); click(140,18);
    check(render().find("Core Database Name") != std::string::npos, "Build form did not open");
    screen.handleEvent(normalizedKey(0,U'Z'));command(Input::CommandCode::Confirm);
    check(render().find("Data/.db")!=std::string::npos,"New Core form accepted input before Tab/hover");
    command(Input::CommandCode::NextFocus); // Newly opened forms start inactive.
    for (char32_t c : std::u32string(U"UI Test"))
    {
        screen.handleEvent(normalizedKey(0,c));
        // ApplicationHost also maps Space to Confirm; this must remain local
        // to the text field instead of starting a build or moving focus.
        if (c == U' ') command(Input::CommandCode::Confirm);
    }
    check(render().find("Data/UI Test.db") != std::string::npos, "Live filename preview failed");
    screen.handleEvent(normalizedKey(0,U'\x7F'));
    check(render().find("Data/UI Tes.db") != std::string::npos, "VT Backspace failed");
    screen.handleEvent(normalizedKey(0x08,U'\b'));
    check(render().find("Data/UI Te.db") != std::string::npos, "Native Backspace failed");
    command(Input::CommandCode::MoveHome);
    check(Input::Detail::vtEditingKey(L"\x1B[3~")==0x2E, "VT Delete sequence was not decoded");
    screen.handleEvent(normalizedKey(Input::Detail::vtEditingKey(L"\x1B[3~"),0));
    check(render().find("Data/I Te.db") != std::string::npos, "Delete at cursor failed");
    screen.handleEvent(normalizedKey(0,U'U'));
    command(Input::CommandCode::MoveEnd);
    screen.handleEvent(normalizedKey(0,U's')); screen.handleEvent(normalizedKey(0,U't'));
    check(render().find("Data/UI Test.db") != std::string::npos, "Editing restoration failed");
    auto move = [&](int x,int y) {
        Input::MouseEvent mouse;mouse.position=Point{x,y};mouse.action=Input::MouseAction::Moved;
        screen.handleEvent(Input::Event::mouse(mouse));render();
    };
    move(8,18);move(120,20);
    command(Input::CommandCode::Confirm);screen.handleEvent(normalizedKey(0,U'X'));
    check(render().find("Data/UI Test.db")!=std::string::npos,"Inactive field accepted typing");
    command(Input::CommandCode::NextFocus);screen.handleEvent(normalizedKey(0,U'X'));
    check(render().find("Data/UI TestX.db")!=std::string::npos,"First Tab did not restore last active input");
    screen.handleEvent(normalizedKey(0x08,U'\b'));
    command(Input::CommandCode::NextFocus);
    command(Input::CommandCode::Confirm);
    check(render().find("PFF_CLASSIC_1989_BASE @ 0.1") != std::string::npos, "Ruleset dropdown did not open");
    move(8,23);move(120,20);
    command(Input::CommandCode::Confirm);
    check(render().find("PFF_CLASSIC_1989_BASE @ 0.1")!=std::string::npos,"Inactive dropdown accepted Confirm");
    command(Input::CommandCode::NextFocus);
    check(render().find("PFF_CLASSIC_1989_BASE @ 0.1")!=std::string::npos,"First Tab closed inactive dropdown");
    command(Input::CommandCode::Confirm); render();
    command(Input::CommandCode::NextFocus);
    command(Input::CommandCode::Confirm); render();
    command(Input::CommandCode::NextFocus); render();
    check(render().find("PFF_CLASSIC_1989_SEASON @ 0.1")==std::string::npos,"Tab did not close the season dropdown");
    command(Input::CommandCode::Confirm);
    const auto buildingView=render();
    check(buildingView.find("REVIEW CORE DATABASE BUILD")!=std::string::npos, "Build results heading changed");
    check(buildingView.find("Build Core Database")==std::string::npos && buildingView.find("Back")==std::string::npos,
        "Intermediate Build/Back actions appeared during construction");
    auto waitBuild = [&] {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
        while(std::chrono::steady_clock::now()<deadline)
        {
            screen.update(Animation::TickEvent{});
            const auto view=render();
            if(view.find("Continue")!=std::string::npos) return view;
            check(view.find("Build Core Database")==std::string::npos && view.find("Back")==std::string::npos,
                "Intermediate action reappeared while building");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("Build result timed out");
    };
    const auto resultView=waitBuild();
    check(resultView.find("SUCCESS:")!=std::string::npos && fs::exists("Data/UI Test.db"),
        "One-click Core build did not publish a database");
    check(resultView.find("REVIEW CORE DATABASE BUILD")!=std::string::npos &&
        resultView.find("Build Core Database")==std::string::npos && resultView.find("Back")==std::string::npos,
        "Build result did not show only Continue");
    const auto savedCore=CoreManifest::bytes("Data/UI Test.db");
    command(Input::CommandCode::Confirm); render();
    check(render().find("REVIEW CORE DATABASE BUILD")==std::string::npos && render().find("Core Database Name")==std::string::npos,
        "Continue did not restore the Commissioner office");
    click(140,18); render();
    check(render().find("Data/.db")!=std::string::npos, "Continue did not clear build state");
    command(Input::CommandCode::NextFocus);
    for(char32_t c:std::u32string(U"UI Test23")) screen.handleEvent(normalizedKey(0,c));
    const fs::path brokenChart="Data/Source/RuleSets/FirstEdition/breakaway.pffdata";
    auto broken=CoreManifest::bytes(brokenChart);
    broken.replace(broken.find("[OUTCOME_0]"),11,"[UNSUPPORTED_FIELD]");
    { std::ofstream out(brokenChart,std::ios::binary); out<<broken; }
    command(Input::CommandCode::NextFocus); command(Input::CommandCode::NextFocus);
    command(Input::CommandCode::NextFocus); command(Input::CommandCode::Confirm); render();
    const auto failedView=waitBuild();
    check(failedView.find("BUILD FAILED")!=std::string::npos && failedView.find("Build Core Database")==std::string::npos &&
        failedView.find("Back")==std::string::npos, "Failed build did not show only Continue");
    command(Input::CommandCode::NextFocus);
    command(Input::CommandCode::MoveEnd);
    check(render().find("Unsupported field")!=std::string::npos, "Malformed chart diagnostic missing");
    check(!fs::exists("Data/UI Test23.db") && !fs::exists("Data/UI Test23.db.building"), "Failed build left artifacts");
    check(CoreManifest::bytes("Data/UI Test.db")==savedCore, "Failed build altered the known-good Core");
    command(Input::CommandCode::Cancel); render();
    check(render().find("Core Database Name")==std::string::npos && dispatches==0, "Continue/cancel changed application navigation");
    // New League: incomplete legacy metadata stays unknown, and selecting an
    // installed Core creates/loads an independently owned immutable snapshot.
    sqlite3* legacy=nullptr;
    check(sqlite3_open("Data/00 Legacy.db",&legacy)==SQLITE_OK,"Cannot create legacy metadata fixture");
    check(sqlite3_exec(legacy,"CREATE TABLE CoreMetadata(MetadataKey TEXT,MetadataValue TEXT); INSERT INTO CoreMetadata VALUES('Season','1989'),('Ruleset','Legacy');",nullptr,nullptr,nullptr)==SQLITE_OK,"Legacy fixture failed");
    sqlite3_close(legacy);
    fs::create_directories("Data/World/Taken League");
    click(140,23);
    check(render().find("NEW LEAGUE")!=std::string::npos,"New League did not open setup");
    check(render().find("Unknown / not recorded")!=std::string::npos,"Missing legacy metadata was invented");
    check(render().find("history not recorded")!=std::string::npos,"Unknown warning history was presented as clean");
    check(render().find("Taken League")!=std::string::npos,"Existing save folders are not shown");
    command(Input::CommandCode::NextFocus);
    for(char32_t c:std::u32string(U"Taken League")) {screen.handleEvent(normalizedKey(0,c));if(c==U' ') command(Input::CommandCode::Confirm);}
    check(render().find("already exists")!=std::string::npos,"Existing name conflict was not shown");
    click(140,18);render();
    check(!fs::exists("Data/World/Taken League/World.db"),"Create overwrote an existing folder");
    // The disabled Create button leaves nothing active; explicitly return to
    // the name field before editing rather than relying on invisible focus.
    click(8,18);render();
    command(Input::CommandCode::MoveHome);
    Input::KeyEvent erase;erase.code=Input::KeyCode::Delete;erase.pressed=true;erase.repeatCount=64;
    screen.handleEvent(Input::Event::key(erase));
    for(char32_t c:std::u32string(U"My League")) {screen.handleEvent(normalizedKey(0,c));if(c==U' ') command(Input::CommandCode::Confirm);}
    check(render().find("My League")!=std::string::npos,"League name spaces failed");
    command(Input::CommandCode::NextFocus);command(Input::CommandCode::Confirm);
    check(render().find("UI Test.db")!=std::string::npos,"Core dropdown did not list built Core");
    command(Input::CommandCode::MoveEnd);command(Input::CommandCode::NextFocus);render();
    check(render().find("00 Legacy.db")==std::string::npos,"Tab did not close the Core dropdown");
    check(render().find("FirstEdition")!=std::string::npos,"Selected Core details did not update");
    check(render().find("PFF_CLASSIC_1989_BASE")!=std::string::npos,"Package metadata missing from details");
    click(10,26);command(Input::CommandCode::MoveEnd);
    check(render().find("WARNING:")!=std::string::npos,"Recorded Core warnings were not displayed");
    click(140,18);render();
    const auto leagueResult=waitBuild();
    check(leagueResult.find("SUCCESS:")!=std::string::npos,"Selected Core could not create a league");
    const fs::path world="Data/World/My League/World.db";
    auto loaded=loadWorldDatabase(world);
    check(loaded.success && loaded.coreSnapshots.size()==1,"Created league snapshot did not load");
    testScheduleCache(world,loaded);
    auto snapshot=loaded.coreSnapshots[0].snapshotPath;
    if(snapshot.is_relative()) snapshot=world.parent_path()/snapshot;
    check(CoreManifest::bytes(snapshot)==savedCore,"League did not preserve the selected Core bytes");
    const auto preservedSnapshot=CoreManifest::bytes(snapshot);
    {std::ofstream out("Data/UI Test.db",std::ios::binary);out<<"Global Core replaced for isolation test";}
    check(loadWorldDatabase(world).success && CoreManifest::bytes(snapshot)==preservedSnapshot,
        "Replacing the global Core broke the saved league");
    const auto oldWorld=CoreManifest::bytes(world);
    const auto duplicate=LeagueSetup::create(LeagueSetup::Core{},"Data/World","My League");
    check(!duplicate.success && CoreManifest::bytes(world)==oldWorld,"Duplicate creation changed a save");
    command(Input::CommandCode::Confirm);render();
    click(140,23);render();command(Input::CommandCode::Cancel);render();
    check(render().find("NEW LEAGUE")==std::string::npos,"Cancel did not leave league setup");
    surface.resize(80, 25); render();
    fs::current_path(previous);
    std::cout << "PASS: Core UI/editing; New League spaces, dropdown/details/warnings, conflicts, creation/load, snapshot isolation, Cancel/reset.\n";
}
#endif

void check(bool passed, const std::string& message)
{
    if (!passed) throw std::runtime_error(message);
}
void write(const fs::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary);
    out << text;
    check(bool(out), "Cannot write test fixture");
}
template<class Callback> void rejects(Callback callback, const std::string& message)
{
    bool rejected = false;
    try { callback(); } catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
int main()
{
    try
    {
        check(CoreManifest::basenameError("My_Core-01").empty(), "Valid basename rejected");
        check(CoreManifest::basenameError("My Core Database").empty(), "Spaces in basename rejected");
        check(CoreManifest::basenameError("My  Core Database").empty(), "Repeated internal spaces rejected");
        for (const auto& name : { "", "CON", "com1", "NUL", "../bad", "bad.db", "bad ", "   ", "CON ", "a:b" })
            check(!CoreManifest::basenameError(name).empty(), "Unsafe basename accepted");
        check(CoreManifest::checksum("hello") == "a430d84680aabd0b", "FNV1a64 vector failed");
        auto source = fs::path("PFF/Data/Source");
        const auto rules = CoreManifest::read(source / "RuleSets/FirstEdition/FirstEdition.pffruleset", "RULESET");
        const auto season = CoreManifest::read(source / "Seasons/1989/1989Season.pffseason", "SEASON");
        check(rules.files.size() == 18 && season.files.size() == 7, "Installed inventory changed");
        const auto fixture = fs::path("bin/ManifestTests") /
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        fs::create_directories(fixture / "Rules/nested");
        fs::create_directories(fixture / "Season");
        const auto manifest = fixture / "Rules/test.pffruleset";
        const auto prefix = std::string("// fixture\n[RULESET_BEGIN]\n[MANIFEST_VERSION] 0.1\n") +
            "[PACKAGE_KEY] TEST_RULES\n[PACKAGE_VERSION] test-1\n[DISPLAY_NAME] Fixture rules\n[PACKAGE_TYPE] RULESET\n[FILES_BEGIN]\n";
        const auto suffix = std::string("[FILES_END]\n[RULESET_END]\n");
        std::string entries;
        for (const auto& file : rules.files)
            if (file.filename() == "result_codes.pffdata" || file.filename() == "offensive_plays.pffdata" ||
                file.filename() == "defensive_plays.pffdata")
            {
                fs::copy_file(file, fixture / "Rules/nested" / file.filename());
                entries += "[FILE] nested/" + file.filename().string() + "\n";
            }
        write(manifest, prefix + entries + suffix);
        check(CoreManifest::read(manifest, "RULESET").files.size() == 3, "Nested relative inputs failed");
        for (const auto& bad : { "../escape.pffdata", "C:/outside.pffdata", "missing.pffdata", "nested/../../escape.pffdata" })
        {
            write(manifest, prefix + "[FILE] " + bad + "\n" + suffix);
            rejects([&] { CoreManifest::read(manifest, "RULESET"); }, "Unsafe/missing file accepted");
        }
        write(manifest, prefix + entries + entries + suffix);
        rejects([&] { CoreManifest::read(manifest, "RULESET"); }, "Duplicate file accepted");
        write(manifest, prefix + entries + "[FILE] nested/OFFENSIVE_PLAYS.pffdata\n" + suffix);
        rejects([&] { CoreManifest::read(manifest, "RULESET"); }, "Windows case alias duplicate accepted");
        write(manifest, prefix + entries);
        rejects([&] { CoreManifest::read(manifest, "RULESET"); }, "Missing closing tags accepted");
        auto wrongVersion = prefix;
        wrongVersion.replace(wrongVersion.find("0.1"), 3, "9.9");
        write(manifest, wrongVersion + entries + suffix);
        rejects([&] { CoreManifest::read(manifest, "RULESET"); }, "Unknown version accepted");
        write(manifest, prefix + entries + suffix);
        write(fixture / "Rules/unlisted.pffdata", "This unlisted file must never be imported.");
        std::string seasonEntries;
        for (const auto& file : season.files)
        {
            fs::copy_file(file, fixture / "Season" / file.filename());
            seasonEntries += "[FILE] " + file.filename().string() + "\n";
        }
        const auto seasonManifest = fixture / "Season/test.pffseason";
        write(seasonManifest, "[SEASON_BEGIN]\n[MANIFEST_VERSION] 0.1\n[PACKAGE_KEY] TEST_SEASON\n"
            "[PACKAGE_VERSION] test-2\n[DISPLAY_NAME] Fixture season\n[PACKAGE_TYPE] SEASON\n"
            "[SEASON_YEAR] 1989\n[FILES_BEGIN]\n" + seasonEntries + "[FILES_END]\n[SEASON_END]\n");
        CoreDatabaseBuildRequest request;
        request.sourceRoot = fixture;
        request.rulesetManifest = rules.path;
        request.seasonManifest = season.path;
        request.outputDatabase = fixture / "Full_Core.db";
        request.validateOnly = true;
        auto result = createCoreDatabaseFromSource(request);
        for (const auto& d : result.diagnostics) if (d.severity == DatabaseDiagnosticSeverity::Error)
            std::cerr << d.sourceFile << ':' << d.sourceLine << ' ' << d.message << '\n';
        check(result.success && result.filesProcessed == 25, "Full 25-file validation failed");
        check(!fs::exists(request.outputDatabase), "Review published a Core");
        request.validateOnly = false;
        check(createCoreDatabaseFromSource(request).success, "Full Core build failed");
        sqlite3* full = nullptr;
        check(sqlite3_open_v2(request.outputDatabase.string().c_str(), &full, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "Cannot inspect full Core");
        auto scalar = [&](const char* sql) {
            sqlite3_stmt* s = nullptr;
            check(sqlite3_prepare_v2(full, sql, -1, &s, nullptr) == SQLITE_OK, "Chart query failed");
            check(sqlite3_step(s) == SQLITE_ROW, "Missing chart result");
            const int value = sqlite3_column_int(s,0); sqlite3_finalize(s); return value;
        };
        check(scalar("SELECT COUNT(*) FROM ChartDefinitions") == 15, "Missing chart family");
        check(scalar("SELECT COUNT(*) FROM ChartRecords WHERE RecordType='PENALTY_RESULT'") == 120, "Missing penalty results");
        check(scalar("SELECT COUNT(*) FROM ChartRecords WHERE RecordType='WILD_PLAY_RESULT'") == 60, "Missing Wild Plays");
        check(scalar("SELECT COUNT(*) FROM ChartRecords WHERE RecordType='SPECIAL_TEAM_PLAY'") == 10, "Missing special teams plays");
        check(scalar("SELECT COUNT(*) FROM ChartScripts WHERE ExecutionStatus='DRAFT'") == 29, "Draft scripts were lost or promoted");
        check(scalar("SELECT COUNT(*) FROM ChartValues WHERE FieldKey='SOURCE_TEXT'") == 59, "Source wording lost");
        check(scalar("SELECT COUNT(*) FROM SourceFiles WHERE Status='IMPORTED'") == 25, "Source import inventory incomplete");
        sqlite3_close(full);
        request.rulesetManifest = manifest;
        request.seasonManifest = seasonManifest;
        request.outputDatabase = fixture / "Named_Core.db";
        request.validateOnly = true;
        result = createCoreDatabaseFromSource(request);
        for (const auto& d : result.diagnostics) if (d.severity == DatabaseDiagnosticSeverity::Error) std::cerr << d.message << '\n';
        check(result.success && result.filesProcessed == 10, "Validate-only supported composition failed");
        check(!fs::exists(request.outputDatabase) && !fs::exists(request.outputDatabase.string() + ".building"), "Validation left output artifacts");
        request.validateOnly = false;
        result = createCoreDatabaseFromSource(request);
        check(result.success && fs::exists(request.outputDatabase), "Named Core publication failed");
        const auto preserved = CoreManifest::bytes(request.outputDatabase);
        check(!createCoreDatabaseFromSource(request).success, "Existing Core was not protected");
        check(CoreManifest::bytes(request.outputDatabase) == preserved, "Existing Core was modified");
        sqlite3* db = nullptr;
        check(sqlite3_open_v2(request.outputDatabase.string().c_str(), &db, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK, "Cannot inspect Core");
        sqlite3_stmt* statement = nullptr;
        check(sqlite3_prepare_v2(db, "SELECT MetadataValue FROM CoreMetadata WHERE MetadataKey='ManifestPackage.0.Key'", -1, &statement, nullptr) == SQLITE_OK, "Provenance query failed");
        check(sqlite3_step(statement) == SQLITE_ROW && std::string(reinterpret_cast<const char*>(sqlite3_column_text(statement, 0))) == "TEST_RULES", "Package provenance lost");
        sqlite3_finalize(statement);
        sqlite3_close(db);
        request.outputDatabase = fixture / "Reserved.db";
        write(fs::path(request.outputDatabase.string() + ".building"), "do not touch");
        check(!createCoreDatabaseFromSource(request).success, "Existing temporary artifact was not protected");
        check(CoreManifest::bytes(fs::path(request.outputDatabase.string() + ".building")) == "do not touch", "Existing temporary artifact was altered");
        // Modder regression: renamed copied manifests may retain 1989 source
        // labels. Neither labels nor historical package sizes approve a build.
        const auto originalSeasonManifest=CoreManifest::bytes(seasonManifest);
        for(const auto& year:{"1990","1991"})
        {
            auto renamed=originalSeasonManifest;
            renamed.replace(renamed.find("[SEASON_YEAR] 1989"),18,std::string("[SEASON_YEAR] ")+year);
            write(seasonManifest,renamed);
            request.outputDatabase=fixture/(std::string("Copied_")+year+".db");
            result=createCoreDatabaseFromSource(request);
            check(result.success,"Renamed copied season was blocked");
            check(std::any_of(result.diagnostics.begin(),result.diagnostics.end(),[](const auto& d){return d.severity==DatabaseDiagnosticSeverity::Warning;}),
                "Copied source-label mismatch should be reported as a warning");
        }
        for(const auto& name:{"players.pffdata","basic_offensive_unit_ratings.pffdata","basic_defensive_unit_ratings.pffdata"})
        {
            const auto path=fixture/"Season"/name;
            auto text=CoreManifest::bytes(path);
            std::size_t pos=0;
            while((pos=text.find("FirstEdition",pos))!=std::string::npos) {text.replace(pos,12,"MyModRules");pos+=10;}
            write(path,text);
        }
        const auto plays=fixture/"Rules/nested/offensive_plays.pffdata";
        auto reduced=CoreManifest::bytes(plays);
        auto first=reduced.find("[OFFENSIVE_PLAY_BEGIN]");
        auto last=reduced.find("[OFFENSIVE_PLAY_END]",first);
        reduced.erase(first,last+std::string("[OFFENSIVE_PLAY_END]").size()-first);
        write(plays,reduced);
        auto chart=CoreManifest::bytes(source/"RuleSets/FirstEdition/breakaway.pffdata");
        first=chart.find("[CHART_ROW_BEGIN]"); last=chart.find("[CHART_ROW_END]",first);
        chart.erase(first,last+std::string("[CHART_ROW_END]").size()-first);
        write(fixture/"Rules/nested/breakaway.pffdata",chart);
        write(manifest,prefix+entries+"[FILE] nested/breakaway.pffdata\n"+suffix);
        request.outputDatabase=fixture/"My_Mod.db";
        result=createCoreDatabaseFromSource(request);
        for(const auto& d:result.diagnostics) if(d.severity==DatabaseDiagnosticSeverity::Error) std::cerr<<d.message<<'\n';
        check(result.success,"Custom ruleset or non-1989 record counts were blocked");
        write(fixture / "Rules/bad.pffruleset", "malformed");
        std::vector<std::string> errors;
        check(CoreManifest::discover(fixture / "Rules", ".pffruleset", "RULESET", errors).size() == 1 && errors.size() == 1,
            "Discovery did not isolate malformed manifest");
        std::cout << "PASS: manifest parsing/discovery, basename safety, all 25 imports, chart/script records, validate-only, named build, provenance, Core/temp protection.\n";
#ifdef CORE_MANIFEST_UI_TESTS
        testScreen(fs::absolute(fixture / "Screen"));
#endif
        return 0;
    }
    catch (const std::exception& ex) { std::cerr << "FAIL: " << ex.what() << '\n'; return 1; }
}
