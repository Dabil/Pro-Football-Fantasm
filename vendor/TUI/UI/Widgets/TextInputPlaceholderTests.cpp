// Standalone rendering regression; excluded from the TUI library project.
// Compile with the existing TUI library from a VS2022 x64 Native Tools prompt:
// cl /std:c++17 /EHsc /MDd /Ivendor\TUI
//   vendor\TUI\UI\Widgets\TextInputPlaceholderTests.cpp x64\Debug\TUI.lib
//   /Febin\TextInputPlaceholderTests.exe /Fobin\TextInputPlaceholderTests.obj
#include "UI/Widgets/TextInput.h"
#include "Rendering/Surface.h"
#include "Input/Event.h"
#include "UI/Widgets/ContainerWidget.h"
#include <memory>
#include <iostream>
#include <stdexcept>

void check(bool passed,const char* message)
{
    if(!passed) throw std::runtime_error(message);
}

int main()
{
    try
    {
        const auto gray=Color::FromRgb(101,101,101);
        const auto red=Color::FromRgb(215,0,0);
        const auto white=Color::FromRgb(255,255,255);
        const auto cream=Color::FromRgb(254,235,180);
        const Style normal=Style().withForeground(white).withBackground(gray);
        const Style focused=Style().withForeground(white).withBackground(red);
        const Style hintNormal=Style().withForeground(cream).withBackground(gray);
        const Style hintFocused=Style().withForeground(Color::FromRgb(0,255,255)).withBackground(red);
        Surface surface(24,1);
        TextInput input(Rect{Point{0,0},Size{24,1}});
        check(input.placeholderNormalStyle()==input.placeholderFocusedStyle(),"Default placeholder behavior changed");
        input.setPlaceholder("Hint text");
        input.setNormalStyle(normal);input.setFocusedStyle(focused);
        input.setPlaceholderNormalStyle(hintNormal);
        input.setPlaceholderFocusedStyle(hintFocused);
        input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0)==hintNormal,"Normal hint style was not rendered");
        check(surface.buffer().getDisplayStyle(1,0).backgroundColorValue()==surface.buffer().getDisplayStyle(20,0).backgroundColorValue(),
            "Normal hint and remaining input have different backgrounds");
        input.setFocused(true);input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0)==hintFocused,"Focused hint style was not rendered");
        check(surface.buffer().getDisplayStyle(1,0).backgroundColorValue()==surface.buffer().getDisplayStyle(20,0).backgroundColorValue(),
            "Focused hint and remaining input have different backgrounds");
        input.setFocused(false);input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0)==hintNormal,"Hint style did not follow loss of focus");
        input.setText("Actual text");input.setFocused(true);input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0)==focused,"Placeholder style leaked into entered text");
        input.clear();
        // Deliberately supply wrong hint backgrounds. They must not be painted,
        // even for fields that never explicitly configure placeholder styles.
        input.setPlaceholderNormalStyle(hintFocused);
        input.setPlaceholderFocusedStyle(hintNormal);
        input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0)==hintNormal.withBackground(red),"Focused hint did not inherit focused field background");
        input.setFocused(false);input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0)==hintFocused.withBackground(gray),"Normal hint did not inherit normal field background");
        input.setDisabledStyle(focused);input.setEnabled(false);input.draw(surface);
        check(surface.buffer().getDisplayStyle(1,0).backgroundColorValue()==surface.buffer().getDisplayStyle(20,0).backgroundColorValue(),
            "Disabled hint background differs from its field");
        TextInput defaults(Rect{Point{0,0},Size{24,1}});
        defaults.setNormalStyle(normal);defaults.setFocusedStyle(focused);defaults.setPlaceholder("Default hint");
        for(bool focus:{false,true}) {
            defaults.setFocused(focus);defaults.draw(surface);
            check(surface.buffer().getDisplayStyle(1,0).backgroundColorValue()==surface.buffer().getDisplayStyle(20,0).backgroundColorValue(),
                "Default hint background differs from its field");
        }
        ContainerWidget root(Rect{Point{0,0},Size{48,1}});
        auto field=std::make_unique<TextInput>(Rect{Point{0,0},Size{24,1}});
        auto* hoverInput=field.get();
        const Style hover=Style().withForeground(white).withBackground(Color::FromRgb(0,80,0));
        hoverInput->setNormalStyle(normal);hoverInput->setFocusedStyle(focused);
        hoverInput->setHoverStyle(hover);hoverInput->setDisabledStyle(normal);
        hoverInput->setPlaceholder("Hover hint");
        root.addChild(std::move(field));
        check(!root.focusedChild(),"Adding a field activated it before user input");
        Input::MouseEvent mouse;mouse.action=Input::MouseAction::Moved;mouse.position=Point{5,0};
        root.handleEvent(Input::Event::mouse(mouse));hoverInput->draw(surface);
        check(hoverInput->isFocused() && root.focusedChild()==hoverInput,"Hover did not take the shared focus");
        check(surface.buffer().getDisplayStyle(20,0)==hover,"Mouse enter did not apply hover style");
        check(surface.buffer().getDisplayStyle(1,0).backgroundColorValue()==hover.backgroundColorValue(),"Hovered hint background mismatched");
        mouse.position=Point{40,0};root.handleEvent(Input::Event::mouse(mouse));hoverInput->draw(surface);
        check(surface.buffer().getDisplayStyle(20,0)==normal && !root.focusedChild(),"Leaving field retained active focus");
        root.draw(surface);check(!root.focusedChild(),"Drawing silently restored focus");
        root.handleEvent(Input::Event::command(Input::Command{Input::CommandCode::NextFocus}));
        check(root.focusedChild()==hoverInput,"Tab did not restore remembered field");
        mouse.position=Point{5,0};root.handleEvent(Input::Event::mouse(mouse));
        root.handleEvent(Input::Event::command(Input::Command{Input::CommandCode::NextFocus}));hoverInput->draw(surface);
        check(surface.buffer().getDisplayStyle(20,0)==focused,"Tab did not restore keyboard styling");
        root.handleEvent(Input::Event::mouse(mouse));hoverInput->draw(surface);
        check(surface.buffer().getDisplayStyle(20,0)==focused,"Stationary mouse reclaimed focus after Tab");
        mouse.position=Point{6,0};root.handleEvent(Input::Event::mouse(mouse));hoverInput->draw(surface);
        check(surface.buffer().getDisplayStyle(20,0)==hover,"Moved mouse did not restore pointer styling");
        root.clearChildFocus();hoverInput->setEnabled(false);hoverInput->draw(surface);
        check(surface.buffer().getDisplayStyle(20,0)==normal,"Disabled field used hover style");
        mouse.position=Point{40,0};root.handleEvent(Input::Event::mouse(mouse));hoverInput->setEnabled(true);hoverInput->draw(surface);
        check(surface.buffer().getDisplayStyle(20,0)==normal,"Disabled field retained stale hover state");
        std::cout<<"PASS: hint backgrounds; shared mouse/keyboard focus, Tab handoff, stationary mouse suppression, disabled styling.\n";
        return 0;
    }
    catch(const std::exception& ex){std::cerr<<"FAIL: "<<ex.what()<<'\n';return 1;}
}
