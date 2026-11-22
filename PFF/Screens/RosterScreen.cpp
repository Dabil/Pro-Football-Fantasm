#include "Screens/RosterScreen.h"
#include "Database/RosterPresentation.h"
#include "Screens/ContentControlFrameView.h"
#include "Screens/GameTimeFormatting.h"
#include "Rendering/Objects/BannerFactory.h"
#include "Rendering/Surface.h"
#include "Utilities/Text/TextClip.h"
#include "Utilities/Text/TextWrap.h"
#include "Input/Event.h"
#include <tuple>

namespace
{
struct RosterCell {std::string text,help; int width=4;};
struct RosterRow {std::vector<RosterCell> cells; std::string title; bool heading=false; std::int64_t player=0;};
const std::vector<RosterCell> common{
    {"POS","Position and depth slot",6},{"St","Player availability status",4},
    {"No","Jersey number",4},{"NAME","Last name, first name",24},
    {"YR","Years in league",4},{"Age","Player age",4},
    {"Ht","Height in feet and inches",7},{"Wt","Weight in pounds",5},{"INJ","Injury rating",4}};
std::string optionalNumber(const std::optional<int>& v){return v?std::to_string(*v):"--";}
}

// Screen-local presentation composition; no TUI public contract changes.
class RosterTable final : public Widget
{
public:
    std::vector<RosterRow> rows;
    Point pointer{-1,-1};
    std::int64_t selected=0;
    int width=0;
    std::string help() const
    {
        if(pointer.y<0||pointer.y>=static_cast<int>(rows.size())||pointer.x<0)return {};
        int x=0;
        for(const auto& cell:rows[pointer.y].cells)
        {if(pointer.x>=x&&pointer.x<x+cell.width)return cell.help;x+=cell.width;}
        return {};
    }
    void draw(Surface& surface) override
    {
        for(int y=0;y<static_cast<int>(rows.size());++y)
        {
            const auto& row=rows[y];
            const auto& style=y==0?PFF_Colors::ScrollBoxHeading:row.heading?PFF_Colors::ScrollBoxSubHeading:
                row.player&&row.player==selected?PFF_Colors::ScrollBoxRowHighlight:
                row.player&&pointer.y==y?PFF_Colors::ScrollBoxRowHover:PFF_Colors::ScrollBoxContentArea;
            surface.buffer().fillRect(Rect{Point{0,y},Size{width,1}},U' ',style);
            if(!row.title.empty())surface.buffer().writeString(0,y,TextClip::clipUtf8Text(row.title,width),style);
            int x=0;
            for(const auto& cell:row.cells)
            {
                surface.buffer().writeString(x,y,TextClip::clipUtf8Text(cell.text,cell.width-1),
                    y==0?PFF_Colors::ScrollBoxHeading:style);
                x+=cell.width;
            }
        }
    }
};

RosterScreen::RosterScreen(Assets::AssetLibrary& assets,std::function<bool(std::string_view)> dispatch,
    std::function<LoadedWorldContext()> context,LoadedWorldContext world,std::int64_t team)
    :m_dispatch(std::move(dispatch)),m_context(std::move(context)),m_selected(std::move(world)),m_teamSeasonId(team)
{
    const auto font=assets.loadBannerFontAsset("banner.ansi_shadow");
    if(font.success&&font.hasFont())m_banner=BannerFactory::makeBanner(*font.asset.font,"Roster",PFF_Colors::ProFootballTitleColor);
    auto list=std::make_unique<ScrollBox>();m_list=list.get();
    auto table=std::make_unique<RosterTable>();m_table=table.get();list->addChild(std::move(table));
    auto styles=list->styleSet();styles.normal=PFF_Colors::ScrollBoxContentArea;styles.focused=styles.normal;
    list->setStyleSet(styles);
    auto bar=list->verticalScrollbarStyle();bar.trackStyle=PFF_Colors::ScrollBoxTrack;bar.thumbStyle=PFF_Colors::ScrollBoxThumb;
    list->setVerticalScrollbarStyle(bar);m_root.addChild(std::move(list));
    auto controls=std::make_unique<ScrollBox>();m_controls=controls.get();
    auto controlStyles=controls->styleSet();
    controlStyles.normal=PFF_Colors::MainBackgroundColor;controlStyles.focused=controlStyles.normal;
    controls->setStyleSet(controlStyles);
    for(const auto* label:{"All","Offense","Defense","Injured","QB","RB","WR","TE","OL","SP","DL","LB","DB"})
    {
        auto button=std::make_unique<Button>(label);
        button->setActivationCallback([this,label](const ButtonActivationResult& a){if(a.activated)choose(label);});
        m_filters.push_back({label,button.get()});controls->addChild(std::move(button));
    }
    m_root.addChild(std::move(controls));
    auto back=std::make_unique<Button>("Return");m_return=back.get();
    back->setActivationCallback([this](const ButtonActivationResult& a){if(a.activated)m_returnPending=true;});
    m_root.addChild(std::move(back));
    m_root.setFocusedChild(*m_controls);
    m_controls->setFocusedChild(*m_filters.front().second);
}
void RosterScreen::onEnter(){refresh();}
void RosterScreen::update(const Animation::TickEvent& e){refresh();m_root.update(e);}
void RosterScreen::refresh()
{
    const auto world=m_context?m_context():m_selected;
    if(m_checked&&world.loaded==m_display.loaded&&world.worldKey==m_display.worldKey&&
        world.databasePath==m_display.databasePath&&world.activeCompetitionSeasonId==m_display.activeCompetitionSeasonId&&
        world.currentTimeUtc==m_display.currentTimeUtc&&world.dataRevision==m_display.dataRevision)return;
    m_display=world;m_checked=true;m_roster={};m_message.clear();
    if(!world.hasActiveCompetitionSeason()||world.worldKey!=m_selected.worldKey||
        world.databasePath!=m_selected.databasePath||world.activeCompetitionSeasonId!=m_selected.activeCompetitionSeasonId)
        m_message="This team is unavailable in the active league and season.";
    else
    {
        m_roster=loadWorldRoster(world.databasePath,world.activeCompetitionSeasonId,m_teamSeasonId);
        if(!m_roster.success)m_message=m_roster.errorMessage;
        else try {for(const auto& p:m_roster.players)PffRoster::status(p);}
        catch(const std::exception& e){m_roster.success=false;m_message=e.what();m_roster.players.clear();}
    }
    rebuildRows();
}
void RosterScreen::choose(const std::string& filter)
{
    if(filter=="All"||filter=="Offense"||filter=="Defense"||filter=="Injured")
    {m_primary=filter;m_controls->viewport().scrollTo(0,0);}
    m_filter=filter;m_table->selected=0;m_mouse={-1,-1};
    rebuildRows();m_list->viewport().scrollTo(0,0);
}
void RosterScreen::rebuildRows()
{
    m_table->rows.clear();
    if(!m_roster.success)return;
    std::vector<const WorldRosterPlayer*> visible;
    for(const auto& p:m_roster.players)if(PffRoster::matches(p,m_filter))visible.push_back(&p);
    std::sort(visible.begin(),visible.end(),[](auto a,auto b){
        return std::make_tuple(PffRoster::groupOrder(PffRoster::group(a->position)),a->position,a->depth,a->lastName,a->firstName)<
            std::make_tuple(PffRoster::groupOrder(PffRoster::group(b->position)),b->position,b->depth,b->lastName,b->firstName);});
    const bool mixed=m_filter=="All"||m_filter=="Offense"||m_filter=="Defense"||m_filter=="Injured";
    std::vector<PffRoster::Column> flatColumns;
    if(!mixed)
        for(const auto* p:visible)
            for(const auto& column:PffRoster::columns(*p))
                if(std::none_of(flatColumns.begin(),flatColumns.end(),[&](const auto& value){return value.key==column.key;}))
                    flatColumns.push_back(column);
    auto first=common;
    if(mixed)first.push_back({"Ratings","Ratings depend on the player card type",56});
    else for(const auto& column:flatColumns)first.push_back({column.label,column.name,4});
    m_table->rows.push_back({first,{},true,0});
    std::string previous;
    for(const auto* p:visible)
    {
        const auto group=PffRoster::group(p->position);
        const auto columns=mixed?PffRoster::columns(*p):flatColumns;
        const auto section=group+" / "+p->cardType;
        if(mixed&&section!=previous)
        {
            previous=section;m_table->rows.push_back({{},section,true,0});
            auto headings=common;
            for(const auto& c:columns)headings.push_back({c.label,c.name,4});
            m_table->rows.push_back({headings,{},true,0});
        }
        auto cells=common;
        const auto status=PffRoster::status(*p);
        const auto height=p->heightInches?std::to_string(*p->heightInches/12)+"'-"+std::to_string(*p->heightInches%12)+"\"":"--";
        const std::vector<std::string> values{p->position+std::to_string(p->depth),status,std::to_string(p->jersey),
            p->lastName+", "+p->firstName,std::to_string(p->yearsInLeague),optionalNumber(p->age),height,
            optionalNumber(p->weightPounds),optionalNumber(p->injuryRating)};
        for(std::size_t i=0;i<cells.size();++i){cells[i].text=values[i];cells[i].help.clear();}
        for(const auto& c:columns){const auto value=PffRoster::rating(*p,c);cells.push_back({value.first,value.second,4});}
        m_table->rows.push_back({cells,{},false,p->playerSeasonId});
    }
    if(visible.empty())m_table->rows.push_back({{},"No players match this filter.",false,0});
    // Measure displayed labels and values, not the tooltip text or the old
    // fixed column allocation. Keep column boundaries aligned across rows.
    std::vector<int> widths(common.size(),0);
    for(std::size_t row=0;row<m_table->rows.size();++row)
        for(std::size_t column=0;column<m_table->rows[row].cells.size();++column)
        {
            if(mixed&&row==0&&column==common.size())continue;
            if(column>=widths.size())widths.resize(column+1,0);
            widths[column]=std::max(widths[column],TextClip::measureDisplayWidth(
                UnicodeConversion::utf8ToU32(m_table->rows[row].cells[column].text))+1);
        }
    int commonWidth=0;
    m_table->width=0;
    for(std::size_t column=0;column<widths.size();++column)
    {m_table->width+=widths[column];if(column<common.size())commonWidth+=widths[column];}
    const int ratingsWidth=std::max(8,m_table->width-commonWidth);
    if(mixed)m_table->width=commonWidth+ratingsWidth;
    for(std::size_t row=0;row<m_table->rows.size();++row)
        for(std::size_t column=0;column<m_table->rows[row].cells.size();++column)
            m_table->rows[row].cells[column].width=mixed&&row==0&&column==common.size()?ratingsWidth:widths[column];
    m_table->setBounds(Rect{Point{0,0},Size{m_table->width,static_cast<int>(m_table->rows.size())}});
    m_list->setContentSize(m_table->width,static_cast<int>(m_table->rows.size()));
}
void RosterScreen::revealPlayer()
{
    const auto selected=std::find_if(m_table->rows.begin(),m_table->rows.end(),
        [this](const auto& row){return row.player&&row.player==m_table->selected;});
    if(selected==m_table->rows.end())return;
    const int row=static_cast<int>(selected-m_table->rows.begin());
    auto& view=m_list->viewport();
    const auto offset=view.scrollOffset();
    const int height=std::max(1,view.viewSize().height);
    if(row<offset.y)view.scrollTo(offset.x,row);
    else if(row>=offset.y+height)view.scrollTo(offset.x,row-height+1);
}
void RosterScreen::movePlayer(int direction)
{
    const auto& rows=m_table->rows;
    auto selected=std::find_if(rows.begin(),rows.end(),
        [this](const auto& row){return row.player&&row.player==m_table->selected;});
    int index=selected==rows.end()?(direction>0?-1:static_cast<int>(rows.size())):
        static_cast<int>(selected-rows.begin());
    for(index+=direction;index>=0&&index<static_cast<int>(rows.size());index+=direction)
        if(rows[index].player){m_table->selected=rows[index].player;break;}
    revealPlayer();
}
void RosterScreen::cycleFocus(int direction)
{
    std::vector<Widget*> order;
    if(m_roster.success)
        for(const auto& item:m_filters)if(item.second->isVisible())order.push_back(item.second);
    order.push_back(m_return);
    if(m_roster.success)order.push_back(m_list);
    Widget* current=m_root.focusedChild();
    if(current==m_controls)current=m_controls->focusedChild();
    const auto found=std::find(order.begin(),order.end(),current);
    const int count=static_cast<int>(order.size());
    const int index=found==order.end()?(direction>0?0:count-1):
        (static_cast<int>(found-order.begin())+direction+count)%count;
    auto* next=order[index];
    m_root.clearPointerState();
    if(next==m_return||next==m_list)m_root.setFocusedChild(*next);
    else
    {
        m_root.setFocusedChild(*m_controls);
        m_controls->setFocusedChild(*next);
        auto& view=m_controls->viewport();
        const auto bounds=next->bounds();
        const auto offset=view.scrollOffset();
        if(bounds.position.y<offset.y)view.scrollTo(offset.x,bounds.position.y);
        else if(bounds.position.y+bounds.size.height>offset.y+view.viewSize().height)
            view.scrollTo(offset.x,bounds.position.y+bounds.size.height-view.viewSize().height);
    }
    if(next==m_list)
    {
        const auto selected=std::find_if(m_table->rows.begin(),m_table->rows.end(),
            [this](const auto& row){return row.player&&row.player==m_table->selected;});
        if(selected==m_table->rows.end())movePlayer(1);
        else revealPlayer();
    }
}
bool RosterScreen::handleEvent(const Input::Event& event)
{
    refresh();
    if(const auto* mouse=event.asMouse())
    {
        m_mouse=mouse->position;
        const auto& bounds=m_list->bounds();
        const auto offset=m_list->viewport().scrollOffset();
        const int row=m_mouse.y-bounds.position.y+offset.y;
        if(mouse->button==Input::MouseButton::Left&&mouse->isPress()&&bounds.contains(m_mouse.x,m_mouse.y)&&
            row>=0&&row<static_cast<int>(m_table->rows.size()))m_table->selected=m_table->rows[row].player;
    }
    else if(event.asKey()||event.asCommand())m_mouse={-1,-1};
    auto command=Input::CommandCode::None;
    if(const auto* value=event.asCommand())command=value->command.code;
    // ApplicationHost delivers the raw key followed by its mapped command.
    // Navigation is performed only for the command, once per key press.
    if(command==Input::CommandCode::NextFocus||command==Input::CommandCode::PreviousFocus)
    {cycleFocus(command==Input::CommandCode::NextFocus?1:-1);return true;}
    if(m_root.focusedChild()==m_list&&
        (command==Input::CommandCode::MoveUp||command==Input::CommandCode::MoveDown))
    {m_root.clearPointerState();movePlayer(command==Input::CommandCode::MoveDown?1:-1);return true;}
    const bool handled=m_root.handleEvent(event);
    if(m_returnPending){m_returnPending=false;const auto dispatch=m_dispatch;if(dispatch)dispatch("nav:back");return true;}
    return handled;
}
void RosterScreen::draw(Surface& surface)
{
    refresh();
    Composition::PageComposer page(surface.buffer());
    const auto frame=ContentControlFrameView::drawFrame(page,m_banner,"Roster");
    const auto& content=frame.content;
    if(content.size.height>0)page.writeLine(content,0,TextClip::clipUtf8Text(
        m_roster.teamName+" - "+m_filter,content.size.width),PFF_Colors::TextHighlightColor);
    m_list->setVisible(m_roster.success);m_controls->setVisible(m_roster.success);
    const int availableHeight=std::max(0,content.size.height-4);
    const int listHeight=std::min(static_cast<int>(m_table->rows.size()),availableHeight);
    const bool verticalScroll=static_cast<int>(m_table->rows.size())>listHeight;
    const int listWidth=std::min(std::max(0,content.size.width-2),m_table->width+(verticalScroll?1:0));
    m_list->setBounds(Rect{Point{content.position.x+(content.size.width-listWidth)/2,content.position.y+2},Size{listWidth,listHeight}});
    if(m_roster.success&&listWidth>0&&listHeight>0)
    {
        const auto bounds=m_list->bounds();
        page.createRegion("RosterPanel",Rect{Point{bounds.position.x-1,bounds.position.y-1},
            Size{listWidth+2,listHeight+2}});
        page.drawPanel("RosterPanel",PFF_Colors::FormPanelColor,PFF_Colors::FormBorderColor,
            ObjectFactory::singleLineBorder());
        page.writePanelTitle("RosterPanel",TextClip::clipUtf8Text(" Roster ",std::max(0,listWidth-2)),
            PFF_Colors::FormPanelHeadingColor,3,0);
    }
    m_list->setVerticalScrollbarVisible(verticalScroll);
    m_list->setContentSize(m_table->width,static_cast<int>(m_table->rows.size()));
    m_controls->setBounds(frame.controls);
    int y=0;
    for(auto& item:m_filters)
    {
        const auto& label=item.first;
        const bool primary=label=="All"||label=="Offense"||label=="Defense"||label=="Injured";
        const bool offense=label=="QB"||label=="RB"||label=="WR"||label=="TE"||label=="OL"||label=="SP";
        const bool show=primary||(m_primary=="Offense"&&offense)||(m_primary=="Defense"&&!offense);
        auto* b=item.second;b->setVisible(show);b->setEnabled(show);
        if(show){b->setBounds(Rect{Point{1,y},Size{std::max(0,frame.controls.size.width-3),3}});y+=3;if(label=="Injured")++y;}
        else b->setBounds(Rect{});
        b->setNormalStyle(label==m_filter?PFF_Colors::ButtonHoverColor:PFF_Colors::ButtonNormalColor);
        b->setFocusedStyle(PFF_Colors::ButtonHoverColor);b->setHoverStyle(PFF_Colors::ButtonHoverColor);
        b->setPressedStyle(PFF_Colors::ButtonPressedColor);
    }
    m_controls->setContentSize(std::max(0,frame.controls.size.width-1),y);
    m_table->pointer={-1,-1};
    if(m_list->bounds().contains(m_mouse.x,m_mouse.y)&&
        m_mouse.x<m_list->bounds().position.x+m_list->bounds().size.width-(verticalScroll&&listWidth>1?1:0))
    {const auto offset=m_list->viewport().scrollOffset();m_table->pointer={m_mouse.x-m_list->bounds().position.x+offset.x,m_mouse.y-m_list->bounds().position.y+offset.y};}
    m_return->setBounds(PffLeagueFooter::returnBounds(frame.footer));
    m_return->setNormalStyle(PFF_Colors::ButtonNormalColor);m_return->setFocusedStyle(PFF_Colors::ButtonHoverColor);
    m_return->setHoverStyle(PFF_Colors::ButtonHoverColor);m_return->setPressedStyle(PFF_Colors::ButtonPressedColor);
    ContentControlFrameView::drawFooter(page,frame.footer,m_display.isLoaded()?m_display.worldName:"No league loaded",
        m_display.isLoaded()?PffGameTime::formatGameTimeEastern(m_display.currentTimeUtc):"");
    if(!m_message.empty()&&content.size.height>2)
    {
        // Wrap diagnostics so rebuild guidance remains readable in a narrow pane.
        const int width=std::max(1,content.size.width-2);
        const auto lines=wrapText(m_message,width);
        for(int line=0;line<content.size.height-2&&line<static_cast<int>(lines.size());++line)
            page.writeLine(content,line+2,lines[line],PFF_Colors::TextHighlightColor);
    }
    if(content.size.height>1&&m_roster.success)
    {
        const auto hint=TextClip::clipUtf8Text("Arrows/Page keys: scroll; hover headings or values for details.",content.size.width);
        const int hintWidth=TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(hint));
        surface.buffer().writeString(content.position.x+(content.size.width-hintWidth)/2,
            content.position.y+content.size.height-1,hint,PFF_Colors::TextHighlightColor);
    }
    m_root.setBounds(frame.screen);m_root.draw(surface);
    const auto help=m_table->help();
    if(!help.empty()&&m_roster.success&&surface.buffer().getWidth()>4&&surface.buffer().getHeight()>3)
    {
        const int width=std::min(TextClip::measureDisplayWidth(UnicodeConversion::utf8ToU32(help))+4,surface.buffer().getWidth());
        const int x=std::clamp(m_mouse.x,0,surface.buffer().getWidth()-width);
        const auto lines=wrapText(help,std::max(1,width-4));
        const int height=std::min(static_cast<int>(lines.size())+2,surface.buffer().getHeight());
        const int top=std::clamp(m_mouse.y+1,0,surface.buffer().getHeight()-height);
        surface.buffer().fillRect(Rect{Point{x,top},Size{width,height}},U' ',PFF_Colors::HeaderInnerColor);
        for(int i=0;i<height-2;++i)surface.buffer().writeString(x+2,top+1+i,
            lines[i],PFF_Colors::TextHighlightColor);
    }
}
