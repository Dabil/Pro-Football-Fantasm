// Standalone shared-focus regression. Build against the PFF vendor TUI library.
#include "UI/Widgets/ContainerWidget.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/TextInput.h"
#include "UI/Widgets/ListBox.h"
#include "Input/Event.h"
#include "Rendering/Surface.h"
#include <iostream>
#include <memory>
#include <stdexcept>

void check(bool passed,const char* message)
{if(!passed) throw std::runtime_error(message);}

int main()
{
    try
    {
        ContainerWidget root(Rect{Point{0,0},Size{60,5}});
        Surface surface(60,5);
        const Style normal=Style().withForeground(Color::FromRgb(255,255,255)).withBackground(Color::FromRgb(60,60,60));
        const Style focused=normal.withBackground(Color::FromRgb(200,0,0));
        const Style hover=normal.withBackground(Color::FromRgb(0,120,0));
        auto makeButton=[&](int x,const char* label) {
            auto button=std::make_unique<Button>(Rect{Point{x,0},Size{12,1}},label);
            button->setNormalStyle(normal);button->setFocusedStyle(focused);button->setHoverStyle(hover);
            auto* ptr=button.get();root.addChild(std::move(button));return ptr;
        };
        auto* a=makeButton(0,"A");
        auto field=std::make_unique<TextInput>(Rect{Point{15,0},Size{12,1}});
        auto* input=field.get();input->setNormalStyle(normal);input->setFocusedStyle(focused);input->setHoverStyle(hover);
        root.addChild(std::move(field));
        auto* b=makeButton(30,"B");
        auto* disabled=makeButton(45,"Disabled");disabled->setEnabled(false);
        auto choices=std::make_unique<ListBox>(Rect{Point{0,2},Size{25,3}});
        auto* list=choices.get();list->addItem("First");list->addItem("Second");
        list->setNormalStyle(normal);list->setFocusedStyle(normal);
        list->setSelectedStyle(hover);list->setSelectedFocusedStyle(focused);
        root.addChild(std::move(choices));
        int activationsA=0,activationsB=0;
        a->setActivationCallback([&](const ButtonActivationResult&){++activationsA;});
        b->setActivationCallback([&](const ButtonActivationResult&){++activationsB;});
        auto active=[&](Widget* expected) {
            int count=0;
            for(std::size_t i=0;i<root.childCount();++i) if(root.childAt(i)->isFocused()) ++count;
            check(count==1 && root.focusedChild()==expected,"More than one focus, or wrong focus owner");
        };
        auto inactive=[&]() { for(std::size_t i=0;i<root.childCount();++i) check(!root.childAt(i)->isFocused(),"Inactive control retained focus"); check(!root.focusedChild(),"Empty space retained active control"); };
        auto mouse=[&](int x,int y,Input::MouseAction action=Input::MouseAction::Moved) {
            Input::MouseEvent event;event.position=Point{x,y};event.action=action;
            if(action!=Input::MouseAction::Moved) event.button=Input::MouseButton::Left;
            root.handleEvent(Input::Event::mouse(event));root.draw(surface);
        };
        auto command=[&](Input::CommandCode code) {root.handleEvent(Input::Event::command(Input::Command{code}));root.draw(surface);};
        inactive();command(Input::CommandCode::PreviousFocus);active(list);
        root.resetFocus();inactive();
        command(Input::CommandCode::NextFocus);active(a);
        mouse(31,0);active(b);
        check(surface.buffer().getDisplayStyle(31,0)==hover && surface.buffer().getDisplayStyle(1,0)==normal,
            "Mouse and keyboard highlights appeared together");
        command(Input::CommandCode::Confirm);
        check(activationsB==1 && activationsA==0,"Confirm did not target only the mouse-focused button");
        command(Input::CommandCode::NextFocus);active(list);
        check(surface.buffer().getDisplayStyle(31,0)==normal,"Tab left hover on the previous button");
        mouse(31,0);active(list); // duplicate report from a stationary pointer
        command(Input::CommandCode::PreviousFocus);active(b);
        check(surface.buffer().getDisplayStyle(31,0)==focused,"Keyboard focus still used hover styling");
        mouse(32,0);active(b);
        check(surface.buffer().getDisplayStyle(31,0)==hover,"Actual motion did not reclaim pointer focus");
        mouse(16,0);inactive();
        mouse(16,0,Input::MouseAction::Clicked);active(input);
        mouse(32,0);active(input);
        Input::KeyEvent text;text.code=Input::KeyCode::Character;text.character=U'x';text.pressed=true;
        root.handleEvent(Input::Event::key(text));check(input->text()=="x","Typing did not reach clicked input after pointer left");
        auto space=text;space.code=Input::KeyCode::Space;
        check(root.handleEvent(Input::Event::key(space)) && input->text()=="x ","Input did not consume Space as text");
        auto backspace=text;backspace.code=Input::KeyCode::Backspace;
        root.handleEvent(Input::Event::key(backspace));check(input->text()=="x","Backspace did not reach focused input");
        command(Input::CommandCode::MoveHome);
        auto del=text;del.code=Input::KeyCode::Delete;
        root.handleEvent(Input::Event::key(del));check(input->text().empty(),"Delete did not reach focused input");
        root.handleEvent(Input::Event::key(text));
        command(Input::CommandCode::Confirm);inactive();
        mouse(17,0);inactive();
        root.handleEvent(Input::Event::key(text));check(input->text()=="x","Input accepted typing after Enter");
        command(Input::CommandCode::NextFocus);active(input);
        command(Input::CommandCode::NextFocus);active(b);
        check(surface.buffer().getDisplayStyle(16,0)==normal,"Input retained hover after Tab");
        mouse(16,0,Input::MouseAction::Clicked);active(input);
        command(Input::CommandCode::PreviousFocus);active(a);
        mouse(46,0);inactive(); // Disabled controls clear pointer focus.
        command(Input::CommandCode::Confirm);
        check(activationsA==0 && activationsB==1,"Disabled hover activated an invisible target");
        mouse(59,4,Input::MouseAction::Clicked);inactive();
        root.handleEvent(Input::Event::key(text));check(input->text()=="x","Inactive input accepted typing");
        command(Input::CommandCode::NextFocus);active(a);
        command(Input::CommandCode::NextFocus);active(input);
        mouse(31,0,Input::MouseAction::Pressed);active(b);
        command(Input::CommandCode::NextFocus);active(list);
        mouse(31,0,Input::MouseAction::Released);active(list);
        check(activationsB==1 && surface.buffer().getDisplayStyle(31,0)==normal,
            "Release after Tab restored stale hover or activated the old button");
        mouse(1,3);active(list);
        check(list->selectedIndex()==std::optional<std::size_t>(0),"Hover changed list selection");
        check(surface.buffer().getDisplayStyle(1,3)==focused && surface.buffer().getDisplayStyle(1,2)==hover,
              "Hover feedback and persistent selection did not draw separately");
        mouse(1,3,Input::MouseAction::Pressed);
        mouse(1,3,Input::MouseAction::Released);
        check(list->selectedIndex()==std::optional<std::size_t>(1),"Click did not select list row");
        mouse(1,2);active(list);
        check(list->selectedIndex()==std::optional<std::size_t>(1),"Crossing another row changed selection");
        mouse(31,0);active(b);
        check(list->selectedIndex()==std::optional<std::size_t>(1),"Leaving list lost selection");
        mouse(1,2,Input::MouseAction::Clicked);active(list);
        check(list->selectedIndex()==std::optional<std::size_t>(0),"Second click did not replace selection");
        command(Input::CommandCode::Confirm);
        check(activationsA==0 && activationsB==1,"Unhandled list Confirm activated an unrelated button");
        mouse(1,0,Input::MouseAction::Pressed);active(a);
        mouse(32,0,Input::MouseAction::Dragged);inactive();
        mouse(32,0,Input::MouseAction::Released);active(b);
        check(activationsA==0 && activationsB==1,"Dragging out activated a different button");
        mouse(1,0,Input::MouseAction::Pressed);active(a);
        mouse(59,4,Input::MouseAction::Released);inactive();
        command(Input::CommandCode::Confirm);inactive();
        root.draw(surface);inactive();
        check(activationsA==0,"Release outside without a move activated the button");
        command(Input::CommandCode::NextFocus);active(a);
        mouse(31,0);active(b);
        mouse(1,4);inactive();command(Input::CommandCode::PreviousFocus);active(b);
        mouse(1,0);active(a);
        root.clearChildFocus();command(Input::CommandCode::NextFocus);active(a);
        a->setVisible(false);command(Input::CommandCode::NextFocus);active(input);
        root.removeChild(*input);check(root.focusedChild()!=nullptr,"Removing active control lost focus recovery");
        // Navigation callbacks can destroy the button and its owning container
        // immediately, just as ScreenManager::popScreen does for Return.
        for (bool keyboard : {false, true})
        {
            auto owner=std::make_unique<ContainerWidget>(Rect{Point{0,0},Size{20,1}});
            auto exit=std::make_unique<Button>(Rect{Point{0,0},Size{20,1}},"Return");
            bool returned=false;
            auto lifetime=std::make_shared<int>(42);
            std::weak_ptr<int> released=lifetime;
            exit->setActivationCallback([&,lifetime](const ButtonActivationResult&) {
                owner.reset();
                check(!released.expired(),"Navigation destroyed its executing callback");
                returned=true;
            });
            lifetime.reset();
            owner->addChild(std::move(exit));
            if(keyboard) {owner->handleEvent(Input::Event::command(Input::Command{Input::CommandCode::NextFocus}));owner->handleEvent(Input::Event::command(Input::Command{Input::CommandCode::Confirm}));}
            else
            {
                Input::MouseEvent click;click.position=Point{1,0};click.button=Input::MouseButton::Left;
                click.action=Input::MouseAction::Pressed;owner->handleEvent(Input::Event::mouse(click));
                click.action=Input::MouseAction::Released;owner->handleEvent(Input::Event::mouse(click));
            }
            check(returned && !owner && released.expired(),"Destructive navigation did not complete safely");
        }
        std::cout<<"PASS: shared focus, Tab/Shift+Tab, real-motion takeover, one highlight, typing/Confirm routing, disabled/hidden controls, rows and capture safety.\n";
        return 0;
    }
    catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
