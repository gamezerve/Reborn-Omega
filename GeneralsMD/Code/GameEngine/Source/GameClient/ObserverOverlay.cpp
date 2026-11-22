// Reborn: Adapt Generals Online's observer table and notifications without changing simulation or player ownership.
#include "PreRTS.h"
#include "GameClient/ObserverOverlay.h"
#include "Common/Energy.h"
#include "Common/CustomControlBar.h" // Reborn: GO observer visuals are exclusive to Control Bar Pro.
#include "Common/GlobalData.h"
#include "Common/Money.h"
#include "Common/NameKeyGenerator.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/PlayerTemplate.h"
#include "Common/ScoreKeeper.h"
#include "Common/SpecialPower.h"
#include "GameClient/ControlBar.h"
#include "GameClient/Display.h"
#include "GameClient/DisplayString.h"
#include "GameClient/DisplayStringManager.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/InGameUI.h"
#include "GameClient/GlobalLanguage.h"
#include "GameClient/GameFont.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"
#include "GameNetwork/GameInfo.h"
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <vector>

namespace
{
    // Reborn: Preserve GO's eight-column table and 300/3000/300 ms notification animation.
    const Int ColumnCount = 8;
    const UnsignedInt SlideMs = 300, VisibleMs = 3000, LifetimeMs = 3600;

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Observer overlays also work for defeated players, while remaining unavailable to active players. */
    //-------------------------------------------------------------------------------------------------
    Bool canObserve()
    {
        return UseCustomControlBar() && TheControlBar && TheControlBar->isObserverControlBarOn() && TheGameLogic &&
            TheGameLogic->isInGame() && !TheGameLogic->isInShellGame() && TheGameLogic->getFrame() > 1;
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Hide observer information behind loading screens, cinematics and the pause/options menu. */
    //-------------------------------------------------------------------------------------------------
    Bool canDraw()
    {
        return canObserve() && TheInGameUI && TheInGameUI->getInputEnabled() &&
            !TheGameLogic->isIntroMoviePlaying() && !TheGameLogic->isLoadingMap() && !TheInGameUI->isQuitMenuVisible();
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Use localized UI captions with safe fallbacks for language packs predating the observer overlay. */
    //-------------------------------------------------------------------------------------------------
    UnicodeString caption(const char* key, const wchar_t* fallback)
    {
        Bool exists = FALSE;
        UnicodeString text = TheGameText ? TheGameText->fetch(key, &exists) : UnicodeString();
        if (!exists) text = fallback;
        return text;
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Remove command accelerators and line breaks from names printed in observer notifications. */
    //-------------------------------------------------------------------------------------------------
    UnicodeString cleanCaption(const UnicodeString& text)
    {
        std::wstring clean(text.str());
        clean.erase(std::remove(clean.begin(), clean.end(), L'&'), clean.end());
        std::replace(clean.begin(), clean.end(), L'\n', L' ');
        std::replace(clean.begin(), clean.end(), L'\r', L' ');
        return UnicodeString(clean.c_str());
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Resolve public powers through their existing command/science captions, including mod-specific powers. */
    //-------------------------------------------------------------------------------------------------
    UnicodeString powerCaption(const SpecialPowerTemplate* power)
    {
        if (!power) return UnicodeString();
        if (TheControlBar)
        {
            AsciiString label = TheControlBar->getSpecialPowerTextLabel(power);
            if (label.isNotEmpty())
            {
                Bool exists = FALSE;
                UnicodeString text = TheGameText->fetch(label, &exists);
                if (exists) return cleanCaption(text);
            }
        }
        UnicodeString name, description;
        if (TheScienceStore && power->getRequiredScience() != SCIENCE_INVALID &&
            TheScienceStore->getNameAndDescription(power->getRequiredScience(), name, description))
            return cleanCaption(name);
        return UnicodeString(); // Reborn: Internal helper powers without a public caption produce no notification.
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Keep all thirteen factions compact in the GO table, with a visible-name fallback for map factions. */
    //-------------------------------------------------------------------------------------------------
    UnicodeString factionCaption(Player* player)
    {
        const char* sides[] = {"America", "AmericaAirForceGeneral", "AmericaSuperWeaponGeneral", "AmericaLaserGeneral",
            "China", "ChinaTankGeneral", "ChinaInfantryGeneral", "ChinaNukeGeneral", "GLA", "GLAToxinGeneral",
            "GLADemolitionGeneral", "GLAStealthGeneral", "Boss"};
        const wchar_t* names[] = {L"USA", L"AFG", L"SWG", L"Laser", L"China", L"Tank", L"Inf", L"Nuke",
            L"GLA", L"Tox", L"Demo", L"Stealth", L"Boss"};
        for (Int i = 0; i < 13; ++i)
            if (player->getSide().compareNoCase(sides[i]) == 0) return UnicodeString(names[i]);
        return player->getPlayerTemplate() ? player->getPlayerTemplate()->getDisplayName() : UnicodeString(L"?");
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Format money and income consistently with the original GO comparison table. */
    //-------------------------------------------------------------------------------------------------
    UnicodeString number(UnsignedInt value)
    {
        std::wstring text = std::to_wstring(value);
        for (Int i = (Int)text.size() - 3; i > 0; i -= 3) text.insert(i, L",");
        return UnicodeString(text.c_str());
    }
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Own all observer fonts, per-player rows and bounded notification history in one UI-only state. */
//-------------------------------------------------------------------------------------------------
struct RebornObserverOverlay::State
{
    struct Row { std::array<UnicodeString, ColumnCount> text; Color color; Int team; Int slot; };
    struct Milestone { Bool initialized = FALSE; Int rank = 1; Bool income = FALSE; Bool power = FALSE; Bool hunted = FALSE; };
    struct Notice { UnicodeString text; Color color; UnsignedInt created; };
    std::array<DisplayString*, ColumnCount> headers = {};
    std::array<std::array<DisplayString*, ColumnCount>, MAX_SLOTS> cells = {};
    DisplayString* notification = nullptr;
    std::vector<Row> rows;
    std::vector<Notice> notices;
    std::map<Int, Milestone> milestones;
    UnsignedInt lastFrame = 0;
    Int statsSize = -1, noticeSize = -1, screenWidth = 0, screenHeight = 0;
    Bool proActive = FALSE; // Reborn: Returning to the original HUD discards Pro-only event history.
    Bool hidden = FALSE;
    Bool statsLayoutDirty = TRUE; // Reborn: Re-evaluate font fitting when rows, resolution or font preferences change.

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Release display strings before the owning UI or resolution resources are destroyed. */
    //-------------------------------------------------------------------------------------------------
    void freeStrings()
    {
        for (auto& text : headers) { if (text && TheDisplayStringManager) TheDisplayStringManager->freeDisplayString(text); text = nullptr; }
        for (auto& row : cells)
            for (auto& text : row) { if (text && TheDisplayStringManager) TheDisplayStringManager->freeDisplayString(text); text = nullptr; }
        if (notification && TheDisplayStringManager) TheDisplayStringManager->freeDisplayString(notification);
        notification = nullptr;
        statsSize = noticeSize = -1;
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Bound notifications like GO and avoid replaying hidden or loading-screen events later. */
    //-------------------------------------------------------------------------------------------------
    void add(Player* player, const UnicodeString& text)
    {
        if (!canDraw() || hidden || !player || !player->isPlayerActive() || player->isPlayerObserver() ||
            TheGlobalData->m_observerNotificationFontSize <= 0 || text.isEmpty()) return;
        const UnsignedInt now = timeGetTime();
        notices.erase(std::remove_if(notices.begin(), notices.end(), [now](const Notice& n) { return now - n.created >= LifetimeMs; }), notices.end());
        if (notices.size() < 8) notices.push_back({text, player->getPlayerColor(), now});
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Gather active players once per logic second and observe every Reborn rank from 2 through 8. */
    //-------------------------------------------------------------------------------------------------
    void update()
    {
        const UnsignedInt frame = TheGameLogic->getFrame();
        if (lastFrame && frame >= lastFrame && frame - lastFrame < LOGICFRAMES_PER_SECOND) return;
        if (frame < lastFrame) { milestones.clear(); notices.clear(); }
        lastFrame = frame;
        rows.clear();
        statsLayoutDirty = TRUE;
        std::map<Int, Bool> builders;
        // Reborn: One world scan covers all factions instead of scanning all objects separately for each player.
        for (Object* object = TheGameLogic->getFirstObject(); object; object = object->getNextObject())
            if (!object->isEffectivelyDead() && object->getControllingPlayer() &&
                (object->isKindOf(KINDOF_DOZER) || (object->isKindOf(KINDOF_COMMANDCENTER) &&
                !object->testStatus(OBJECT_STATUS_UNDER_CONSTRUCTION))))
                builders[object->getControllingPlayer()->getPlayerIndex()] = TRUE;
        for (Int slotIndex = 0; slotIndex < MAX_SLOTS; ++slotIndex)
        {
            Player* player = ThePlayerList->getPlayerFromSlotIndex(slotIndex);
            if (!player || !player->isPlayerActive() || player->isPlayerObserver()) continue;
            const GameSlot* slot = TheGameInfo ? TheGameInfo->getConstSlot(slotIndex) : nullptr;
            const Money* money = player->getMoney();
            const Energy* energy = player->getEnergy();
            ScoreKeeper* score = player->getScoreKeeper();
            const Int team = slot ? slot->getTeamNumber() : -1;
            const UnsignedInt cash = money ? money->countMoney() : 0;
            const UnsignedInt income = money ? money->getCashPerMinute() : 0;
            const Int rank = player->getRankLevel();
            const Bool gla = player->getBaseSide().compareNoCase("GLA") == 0;
            const Bool powered = energy && energy->getProduction() > 0;
            const Bool hunted = !gla && !builders[player->getPlayerIndex()];
            Milestone& previous = milestones[player->getPlayerIndex()];
            // Reborn: Initial snapshots and scripted opening ranks are baselines, not new observer events.
            if (previous.initialized && TheGlobalData->m_observerNotificationMilestone && frame >= 10 * LOGICFRAMES_PER_SECOND)
            {
                UnicodeString text;
                for (Int level = previous.rank + 1; level <= rank; ++level)
                {
                    text.format(caption("GUI:ObserverRank", L"%ls reached Rank %d").str(), player->getPlayerDisplayName().str(), level);
                    add(player, text);
                }
                if (!previous.income && income >= 10000)
                {
                    text.format(caption("GUI:ObserverIncome", L"%ls reached 10k/min income").str(), player->getPlayerDisplayName().str()); add(player, text);
                }
                if (gla && !previous.power && powered)
                {
                    text.format(caption("GUI:ObserverPower", L"%ls now has power").str(), player->getPlayerDisplayName().str()); add(player, text);
                }
                if (!previous.hunted && hunted)
                {
                    text.format(caption("GUI:ObserverHunted", L"%ls has no dozer or completed Command Center left").str(), player->getPlayerDisplayName().str()); add(player, text);
                }
            }
            previous.initialized = TRUE; previous.rank = rank;
            previous.income = previous.income || income >= 10000;
            previous.power = previous.power || powered; previous.hunted = previous.hunted || hunted;
            Row row; row.color = player->getPlayerColor(); row.team = team; row.slot = slotIndex;
            std::wstring name(player->getPlayerDisplayName().str());
            if (name.size() > 12) name = name.substr(0, 12) + L".";
            if (team >= 0) row.text[0].format(L"(%d) %ls", team + 1, name.c_str()); else row.text[0] = name.c_str();
            row.text[1] = factionCaption(player); row.text[2] = number(cash);
            row.text[3].format(L"+%ls", number(income).str());
            row.text[4].format(L"(%d) %d", rank, player->getSkillPoints());
            row.text[5].format(L"%d", player->getSciencePurchasePoints());
            const Int kills = score ? score->getTotalUnitsDestroyed() : 0, deaths = score ? score->getTotalUnitsLost() : 0;
            row.text[6].format(L"%.1f", deaths > 0 ? (Real)kills / deaths : (Real)kills);
            if (energy && (energy->getProduction() || energy->getConsumption()))
                row.text[7].format(energy->hasSufficientPower() ? L"ON (%d)" : L"OFF (%d)", energy->getProduction() - energy->getConsumption());
            else row.text[7] = L"-";
            rows.push_back(row);
        }
        std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.team < b.team; });
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Recreate only display resources when fonts or resolution change, preserving match event history. */
    //-------------------------------------------------------------------------------------------------
    void prepareStrings()
    {
        if (statsSize == TheGlobalData->m_observerStatsFontSize && noticeSize == TheGlobalData->m_observerNotificationFontSize &&
            screenWidth == TheDisplay->getWidth() && screenHeight == TheDisplay->getHeight()) return;
        freeStrings();
        statsLayoutDirty = TRUE;
        statsSize = TheGlobalData->m_observerStatsFontSize; noticeSize = TheGlobalData->m_observerNotificationFontSize;
        screenWidth = TheDisplay->getWidth(); screenHeight = TheDisplay->getHeight();
        const char* keys[] = {"GUI:ObserverPlayer", "GUI:ObserverArmy", "GUI:ObserverCash", "GUI:ObserverCashPerMinute", "GUI:ObserverXP", "GUI:ObserverSP", "GUI:ObserverKD", "GUI:ObserverEnergy"};
        const wchar_t* labels[] = {L"(T) Player", L"Army", L"Cash", L"Cash/m", L"(R) XP", L"SP", L"K/D", L"Power"};
        if (statsSize > 0)
        {
            GameFont* font = TheWindowManager->winFindFont("Tahoma", TheGlobalLanguageData ? TheGlobalLanguageData->adjustFontSize(statsSize) : statsSize, TRUE);
            for (Int column = 0; column < ColumnCount; ++column)
            {
                headers[column] = TheDisplayStringManager->newDisplayString(); headers[column]->setFont(font); headers[column]->setText(caption(keys[column], labels[column]));
                for (auto& row : cells) { row[column] = TheDisplayStringManager->newDisplayString(); row[column]->setFont(font); }
            }
        }
        if (noticeSize > 0)
        {
            notification = TheDisplayStringManager->newDisplayString();
            notification->setFont(TheWindowManager->winFindFont("Tahoma", TheGlobalLanguageData ? TheGlobalLanguageData->adjustFontSize(noticeSize) : noticeSize, TRUE));
        }
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Draw a team-sorted comparison table without covering the existing player card or science panel. */
    //-------------------------------------------------------------------------------------------------
    void drawStats()
    {
        if (statsSize <= 0 || rows.empty()) return;
        GameWindow* science = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("GeneralsExpPoints.wnd:GenExpParent"));
        if (science && !science->winIsHidden()) return;
        // Reborn: Keep the table against the left edge of the selected-unit frame in every observer selection state.
        Int portraitX = screenWidth * 1642 / 1920, portraitY = 0;
        GameWindow* portrait = TheWindowManager->winGetWindowFromId(nullptr, NAMEKEY("ControlBar.wnd:RightHUD"));
        if (portrait) portrait->winGetScreenPosition(&portraitX, &portraitY);
        const Int rightEdge = std::min(screenWidth, portraitX - std::max(1, screenWidth * 25 / 1920));
        const Int availableWidth = std::max(1, rightEdge - screenWidth * 402 / 1920);
        const Int pad = std::max(4, screenWidth / 192), spacing = std::max(8, screenWidth / 120);
        std::array<Int, ColumnCount> widths = {};
        for (size_t row = 0; row < rows.size(); ++row)
            for (Int col = 0; col < ColumnCount; ++col) cells[row][col]->setText(rows[row].text[col]);
        auto measure = [&]() {
            Int total = pad * 2;
            for (Int col = 0; col < ColumnCount; ++col)
            {
                widths[col] = headers[col]->getWidth();
                for (size_t row = 0; row < rows.size(); ++row) widths[col] = std::max(widths[col], cells[row][col]->getWidth());
                widths[col] += spacing; total += widths[col];
            }
            return total;
        };
        // Reborn: Restore the requested font after long rows disappear, without rebuilding text on every render frame.
        if (statsLayoutDirty)
        {
            GameFont* font = TheWindowManager->winFindFont("Tahoma", TheGlobalLanguageData ? TheGlobalLanguageData->adjustFontSize(statsSize) : statsSize, TRUE);
            for (auto* header : headers) header->setFont(font);
            for (auto& row : cells) for (auto* text : row) text->setFont(font);
            statsLayoutDirty = FALSE;
        }
        Int width = measure();
        // Reborn: Large requested fonts still fit narrow displays instead of clipping the last columns.
        if (width > availableWidth)
        {
            GameFont* font = TheWindowManager->winFindFont("Tahoma", std::max(5, (TheGlobalLanguageData ? TheGlobalLanguageData->adjustFontSize(statsSize) : statsSize) * availableWidth / width), TRUE);
            for (auto* header : headers) header->setFont(font);
            for (auto& row : cells) for (auto* text : row) text->setFont(font);
            width = measure();
        }
        const Int step = headers[0]->getFont()->height + 4;
        const Int height = step * ((Int)rows.size() + 1) + pad * 2;
        // Reborn: Player selection must not move the table upward; its bottom and portrait-side anchor stay fixed.
        const Int x = std::max(0, rightEdge - width);
        const Int y = std::max(0, screenHeight - height);
        const Color border = GameMakeColor(255, 255, 255, 110), shadow = GameMakeColor(0, 0, 0, 255);
        TheWindowManager->winFillRect(GameMakeColor(0, 0, 0, 150), 1, x, y, x + width, y + height);
        TheWindowManager->winOpenRect(border, 1, x, y, x + width, y + height);
        Int left = x + pad;
        for (Int col = 0; col < ColumnCount; ++col)
        {
            headers[col]->draw(left, y + pad, GameMakeColor(255, 255, 255, 255), shadow);
            for (size_t row = 0; row < rows.size(); ++row) cells[row][col]->draw(left, y + pad + step * ((Int)row + 1), rows[row].color, shadow);
            left += widths[col];
            if (col + 1 < ColumnCount) TheWindowManager->winFillRect(border, 1, left - spacing / 2, y, left - spacing / 2 + 1, y + height);
        }
        TheWindowManager->winFillRect(border, 1, x, y + pad + step - 2, x + width, y + pad + step - 1);
    }

    //-------------------------------------------------------------------------------------------------
    /** Reborn: Animate bounded observer notices with GO timing and brighter player colors. */
    //-------------------------------------------------------------------------------------------------
    void drawNotices()
    {
        if (!notification) return;
        const UnsignedInt now = timeGetTime();
        notices.erase(std::remove_if(notices.begin(), notices.end(), [now](const Notice& n) { return now - n.created >= LifetimeMs; }), notices.end());
        const Int height = notification->getFont()->height + 16;
        for (size_t i = 0; i < notices.size(); ++i)
        {
            const Notice& notice = notices[i]; const UnsignedInt age = now - notice.created;
            Real fraction = age < SlideMs ? (Real)age / SlideMs : age < SlideMs + VisibleMs ? 1.0f : 1.0f - (Real)(age - SlideMs - VisibleMs) / SlideMs;
            fraction = std::max(0.0f, std::min(1.0f, fraction)); fraction = 1.0f - (1.0f - fraction) * (1.0f - fraction);
            notification->setText(notice.text);
            const Int width = std::min(screenWidth - 14, notification->getWidth() + 24);
            const Int x = 7 - (Int)((1.0f - fraction) * (width + 7));
            const Int y = std::max(8, screenHeight / 2 - screenHeight * 300 / 1080) + (Int)i * (height + 5);
            UnsignedByte r, g, b, a; GameGetColorComponents(notice.color, &r, &g, &b, &a);
            const Color color = GameMakeColor(r + (255 - r) * 0.4f, g + (255 - g) * 0.4f, b + (255 - b) * 0.4f, 255);
            TheWindowManager->winFillRect(GameMakeColor(0, 0, 0, 90), 1, x, y, x + width, y + height);
            TheWindowManager->winOpenRect(GameMakeColor(255, 255, 255, 90), 1, x, y, x + width, y + height);
            IRegion2D clip = { {x + 1, y + 1}, {x + width - 1, y + height - 1} };
            notification->setClipRegion(&clip);
            notification->draw(x + 12, y + 8, color, GameMakeColor(0, 0, 0, 255));
        }
    }
};

//-------------------------------------------------------------------------------------------------
/** Reborn: Start with no graphics resources so startup ordering cannot create observer fonts too early. */
//-------------------------------------------------------------------------------------------------
RebornObserverOverlay::RebornObserverOverlay() : m_state(new State) {}

//-------------------------------------------------------------------------------------------------
/** Reborn: Release observer display strings while the engine's display-string manager still exists. */
//-------------------------------------------------------------------------------------------------
RebornObserverOverlay::~RebornObserverOverlay() { m_state->freeStrings(); delete m_state; }

//-------------------------------------------------------------------------------------------------
/** Reborn: New matches and loaded replays start with fresh notification baselines and no stale player rows. */
//-------------------------------------------------------------------------------------------------
void RebornObserverOverlay::reset() { m_state->freeStrings(); delete m_state; m_state = new State; }

//-------------------------------------------------------------------------------------------------
/** Reborn: Preserve GO's single HUD toggle for the table and notices, without changing their saved font sizes. */
//-------------------------------------------------------------------------------------------------
void RebornObserverOverlay::toggle() { if (!UseCustomControlBar()) return; m_state->hidden = !m_state->hidden; if (m_state->hidden) m_state->notices.clear(); }

//-------------------------------------------------------------------------------------------------
/** Reborn: Refresh spectator-only information without emitting or changing any simulation commands. */
//-------------------------------------------------------------------------------------------------
void RebornObserverOverlay::draw()
{
    // Reborn: Original HUD keeps its legacy information; no Pro notices or milestones should accumulate there.
    if (!UseCustomControlBar())
    {
        if (m_state->proActive) reset();
        return;
    }
    m_state->proActive = TRUE;
    if (!canObserve() || !ThePlayerList || !TheDisplay || !TheWindowManager || !TheDisplayStringManager) return;
    m_state->update();
    if (!canDraw() || m_state->hidden) return;
    m_state->prepareStrings(); m_state->drawStats(); m_state->drawNotices();
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Report successful science purchases using their actual visible names. */
//-------------------------------------------------------------------------------------------------
void RebornObserverOverlay::notifyScience(Player* player, ScienceType science)
{
    if (!canObserve() || !player || !TheGlobalData->m_observerNotificationSpecialPowerPurchase || !TheScienceStore) return;
    UnicodeString name, description, text;
    if (!TheScienceStore->getNameAndDescription(science, name, description)) return;
    text.format(caption("GUI:ObserverPurchased", L"%ls purchased %ls").str(), player->getPlayerDisplayName().str(), cleanCaption(name).str());
    m_state->add(player, text);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Report visible special powers by their public captions rather than a hardcoded retail power list. */
//-------------------------------------------------------------------------------------------------
void RebornObserverOverlay::notifyPower(Player* player, const SpecialPowerTemplate* power)
{
    if (!canObserve() || !player || !TheGlobalData->m_observerNotificationSpecialPowerUsage) return;
    const UnicodeString name = powerCaption(power);
    if (name.isEmpty()) return;
    UnicodeString text; text.format(caption("GUI:ObserverUsed", L"%ls used %ls").str(), player->getPlayerDisplayName().str(), name.str());
    m_state->add(player, text);
}
