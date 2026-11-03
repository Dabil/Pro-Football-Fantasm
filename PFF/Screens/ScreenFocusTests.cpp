// Standalone screen-level regression; excluded from the PFF application.
#include "Screens/MainScreen.h"
#include "Screens/LeaguePlayScreen.h"
#include "Screens/LeagueLeadersScreen.h"
#include "Screens/TradeDeskScreen.h"
#include "Screens/TeamsAndStandingsScreen.h"
#include "Screens/LeagueScheduleScreen.h"
#include "Input/Event.h"
#include "Rendering/Surface.h"
#include "Styles/PFF_Theme.h"
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    void check(bool passed,const std::string& message)
    {if(!passed) throw std::runtime_error(message);}

    struct Control {std::string label,command;};

    void verify(Screen& screen,const std::vector<Control>& order,std::vector<std::string>& actions)
    {
        Surface surface(160,55);
        auto render=[&] {screen.draw(surface);};
        auto command=[&](Input::CommandCode code) {
            screen.handleEvent(Input::Event::command(Input::Command{code}));render();
        };
        auto position=[&](const std::string& label) {
            for(int y=0;y<55;++y)
            {
                std::string row;
                for(int x=0;x<160;++x) row+=static_cast<char>(surface.buffer().getDisplayGlyph(x,y));
                const auto x=row.find(label);
                if(x!=std::string::npos) return Point{static_cast<int>(x),y};
            }
            throw std::runtime_error("Missing button: "+label);
        };
        auto focused=[&](std::size_t expected) {
            for(std::size_t i=0;i<order.size();++i)
            {
                const Point p=position(order[i].label);
                check(surface.buffer().getDisplayStyle(p.x,p.y)==(i==expected
                    ?PFF_Colors::ButtonHoverColor:PFF_Colors::ButtonNormalColor),
                    "Wrong focus highlight: "+order[i].label);
            }
        };
        auto mouse=[&](Point p) {
            Input::MouseEvent event;event.action=Input::MouseAction::Moved;event.position=p;
            screen.handleEvent(Input::Event::mouse(event));render();
        };
        render();focused(order.size());
        screen.onEnter();render();focused(order.size());
        const auto initialCount=actions.size();command(Input::CommandCode::Confirm);
        check(actions.size()==initialCount,"New screen activated a control before Tab/hover");
        command(Input::CommandCode::NextFocus);focused(0);
        for(std::size_t i=0;i<order.size()*2;++i)
        {
            command(Input::CommandCode::NextFocus);
            const auto active=(i+1)%order.size();focused(active);
            const auto count=actions.size();command(Input::CommandCode::Confirm);
            check(actions.size()==count+1 && actions.back()==order[active].command,
                "Enter activated the wrong control: "+order[active].label);
        }
        command(Input::CommandCode::PreviousFocus);focused(order.size()-1);
        command(Input::CommandCode::NextFocus);focused(0);
        mouse(position(order[0].label));focused(0);
        mouse(Point{0,0});focused(order.size()); // No active control.
        const auto count=actions.size();command(Input::CommandCode::Confirm);
        check(actions.size()==count,"Inactive Enter activated a control");
        command(Input::CommandCode::NextFocus);focused(0);
        command(Input::CommandCode::NextFocus);focused(1%order.size());
        screen.onExit();screen.onEnter();render();focused(order.size());
        screen.onExit();
    }
}

int main()
{
    try
    {
        Assets::AssetLibrary assets;
        std::vector<std::string> actions;
        auto dispatch=[&](std::string_view command) {actions.emplace_back(command);return true;};
        auto context=[] {return LoadedWorldContext{};};
        MainScreen main(assets,dispatch,[] {return true;});
        verify(main,{{"League Play","route:pff:league_play"},
            {"Commissioner's Office","route:pff:commissioners_office"}},actions);
        MainScreen unloaded(assets,dispatch,[] {return false;});
        verify(unloaded,{{"Commissioner's Office","route:pff:commissioners_office"}},actions);
        LeaguePlayScreen play(assets,dispatch);
        verify(play,{{"Return","nav:back"},{"Teams & Standings","route:pff:teams_and_standings"},
            {"League Schedule","route:pff:league_schedule"},{"League Leaders","route:pff:league_leaders"},
            {"Trade Desk","route:pff:trade_desk"}},actions);
        LeagueLeadersScreen leaders(assets,dispatch);
        verify(leaders,{{"Return","nav:back"}},actions);
        TradeDeskScreen trades(assets,dispatch);
        verify(trades,{{"Return","nav:back"}},actions);
        TeamsAndStandingsScreen teams(assets,dispatch,context);
        verify(teams,{{"Return","nav:back"}},actions);
        LoadedWorldContext changingContext;
        int contextReads=0;
        TeamsAndStandingsScreen cached(assets,dispatch,[&] {++contextReads;return changingContext;});
        Surface cachedSurface(160,55);
        cached.onEnter();cached.draw(cachedSurface);
        const ScreenBuffer firstFrame=cachedSurface.buffer();
        cached.draw(cachedSurface);
        check(firstFrame.renderToUtf8String()==cachedSurface.buffer().renderToUtf8String(),
            "Cached standings changed displayed text");
        for(int y=0;y<55;++y) for(int x=0;x<160;++x)
            check(firstFrame.getDisplayStyle(x,y)==cachedSurface.buffer().getDisplayStyle(x,y),
                "Cached standings changed displayed styles");
        const int readsBeforeInput=contextReads;
        cached.handleEvent(Input::Event::command(Input::Command{Input::CommandCode::NextFocus}));
        cached.handleEvent(Input::Event::command(Input::Command{Input::CommandCode::Confirm}));
        check(contextReads==readsBeforeInput,"Return performed league refresh work before dispatch");
        changingContext.loaded=true;changingContext.worldName="Changed league";
        cached.draw(cachedSurface);
        check(cachedSurface.buffer().renderToUtf8String().find("Changed league")!=std::string::npos,
            "Standings cache ignored context change");
        cachedSurface.resize(120,48);cached.draw(cachedSurface);
        check(cachedSurface.buffer().renderToUtf8String().find("Return")!=std::string::npos,
            "Standings cache ignored resize");
        LeagueScheduleScreen schedule(assets,dispatch,context);
        verify(schedule,{{"Return","nav:back"}},actions);
        std::cout<<"PASS: every other control-bearing screen: visible focus, Tab wrap, Shift+Tab, Enter routing, unavailable controls, mouse exit, and focus restoration.\n";
        return 0;
    }
    catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
}
