/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////
// FILE: ChaptersMenu.cpp ////////////////////////////////////////////////////////////////////
// Author: Chris Brue, August 2002
// Description: 
///////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ChapterProgress.h"
#include "Common/file.h"
#include "Common/FileSystem.h"
#include "Common/GameEngine.h"
#include "Common/GameState.h"
#include "Common/GlobalData.h"
#include "Common/MessageStream.h"
#include "Common/NameKeyGenerator.h"
#include "Common/OptionPreferences.h"
#include "Common/PlayerTemplate.h"
#include "Common/RandomValue.h"
#include "GameClient/CampaignManager.h"
#include "GameClient/Display.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetListBox.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameText.h"
#include "GameClient/GameWindow.h"
#include "GameClient/GameWindowManager.h"
#include "GameClient/GameWindowTransitions.h"
#include "GameClient/MapUtil.h"
#include "GameClient/Shell.h"
#include "GameClient/WindowLayout.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/ScriptEngine.h"

#include <fstream>
#include <string>
#include <vector>
#include <cctype>

static NameKeyType buttonBackID = NAMEKEY_INVALID;
static NameKeyType buttonStartID = NAMEKEY_INVALID;
static NameKeyType radioEasyID = NAMEKEY_INVALID;
static NameKeyType radioMediumID = NAMEKEY_INVALID;
static NameKeyType radioHardID = NAMEKEY_INVALID;

static GameDifficulty selectedDifficulty = DIFFICULTY_NORMAL;


static Int initialGadgetDelay = 2;
static Bool justEntered = FALSE;
static Bool buttonPushed = FALSE;
static Bool chapterImagesCached = FALSE;
static Bool startGame = FALSE;
static Bool isShuttingDown = FALSE;
static Bool ignoreNextMapSelectionForDoubleClick = FALSE;
static Bool resetDifficultyOnNextInit = FALSE;
static Bool resetChapterImagesOnNextInit = FALSE;
static Bool missionLaunchPending = FALSE;
static Bool s_chaptersMenuUsesGeneralsLayout = FALSE; // Reborn: Track the Chapters layout that is actually alive.

Bool g_chapterMissionLaunchActive = FALSE;

static UnsignedInt lastMapClickTime = 0;
static Int lastMapClickRow = -1;

static GameWindow* chapterButtons[8];
static Int selectedChapter = -1;

static const Image* normalImages[8][3];
static const Image* selectedImages[8][3];

static Color normalTextColor[8];
static Color normalTextBorderColor[8];
static Color normalHiliteTextColor[8];
static Color normalHiliteTextBorderColor[8];

static Color selectedTextColor[8];
static Color selectedTextBorderColor[8];

static const Image* normalHiliteImages[8][3];

struct CampaignMissionInfo
{
  std::string mapPath;
  std::string displayNameLabel;
  std::string missionName;
  std::string playerFaction;
  std::string allyFaction1;
  std::string enemyFaction1;
  std::string enemyFaction2;
  Int worldMapMarkerX = -1;
  Int worldMapMarkerY = -1;
};

static std::vector<CampaignMissionInfo> campaignMissionData;

static Int lastSelectedMapRow = 0;

static GameWindow* worldMapMarker = nullptr;
static UnsignedInt worldMapMarkerLastFrameTime = 0;
static Int worldMapMarkerFrame = 0;
static const Image* worldMapMarkerImages[20] = { nullptr };

//-------------------------------------------------------------------------------------------------
/** Reborn: Resolve a Chapters child ID against the currently loaded visual theme. */
//-------------------------------------------------------------------------------------------------
static NameKeyType GetChaptersMenuWindowKey(const char* windowName)
{
  const char* childName = strchr(windowName, ':');
  childName = childName ? childName + 1 : windowName;

  AsciiString themedName;
  themedName.format(
    "%s:%s",
    s_chaptersMenuUsesGeneralsLayout ? "ChaptersMenuGen.wnd" : "ChaptersMenu.wnd",
    childName);
  return TheNameKeyGenerator->nameToKey(themedName.str());
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Select the transition group that targets the active Chapters window prefix. */
//-------------------------------------------------------------------------------------------------
static const char* GetChaptersMenuTransitionGroup()
{
  return s_chaptersMenuUsesGeneralsLayout ? "ChaptersMenuFadeGen" : "ChaptersMenuFade"; // Reborn: Keep transitions bound to the active prefix.
}

static const Image* GetFactionIcon(const std::string& factionName)
{
  if (factionName.empty())
    return nullptr;

  const PlayerTemplate* playerTemplate = ThePlayerTemplateStore->findPlayerTemplate(TheNameKeyGenerator->nameToKey(factionName.c_str()));

  if (!playerTemplate)
    return nullptr;

  return playerTemplate->getSideIconImage();
}

static void SetWindowImage(const char* windowName, const Image* image)
{
  GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey(windowName));

  if (!window)
    return;

  if (image)
  {
    window->winSetStatus(WIN_STATUS_IMAGE);
    window->winSetEnabledImage(0, image);
    window->winHide(FALSE);
  }
  else
  {
    window->winHide(TRUE);
  }
}

static void PositionDifficultyStarsForRadio(const char* radioName, const char* leftStarName, const char* rightStarName)
{
  GameWindow* radio = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey(radioName)
  );

  GameWindow* leftStar = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey(leftStarName)
  );

  GameWindow* rightStar = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey(rightStarName)
  );

  if (!radio || !leftStar || !rightStar)
    return;

  Int radioX, radioY;
  radio->winGetPosition(&radioX, &radioY);

  ICoord2D radioSize;
  radio->winGetSize(&radioSize.x, &radioSize.y);

  Real scaleX = (Real)radioSize.x / 128.0f;
  Real scaleY = (Real)radioSize.y / 24.0f;

  Int leftX = radioX + REAL_TO_INT_FLOOR(18.0f * scaleX);
  Int rightX = radioX + REAL_TO_INT_FLOOR(93.0f * scaleX);
  Int starY = radioY + REAL_TO_INT_FLOOR(4.0f * scaleY);

  leftStar->winSetPosition(leftX, starY);
  rightStar->winSetPosition(rightX, starY);
}

static void PositionDifficultyStars()
{
  PositionDifficultyStarsForRadio(
    "ChaptersMenu.wnd:RadioButtonEasyAI",
    "ChaptersMenu.wnd:EasyLeftStar",
    "ChaptersMenu.wnd:EasyRightStar"
  );

  PositionDifficultyStarsForRadio(
    "ChaptersMenu.wnd:RadioButtonMediumAI",
    "ChaptersMenu.wnd:MediumLeftStar",
    "ChaptersMenu.wnd:MediumRightStar"
  );

  PositionDifficultyStarsForRadio(
    "ChaptersMenu.wnd:RadioButtonHardAI",
    "ChaptersMenu.wnd:HardLeftStar",
    "ChaptersMenu.wnd:HardRightStar"
  );
}

static void PositionMissionFactionIcons(Bool hasAlly, Bool hasSecondEnemy)
{
  GameWindow* playerWindow = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:PlayerFactionIcon")
  );

  GameWindow* allyWindow = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:AllyFactionIcon1")
  );

  GameWindow* enemyWindow1 = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:EnemyFactionIcon1")
  );

  GameWindow* enemyWindow2 = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:EnemyFactionIcon2")
  );

  GameWindow* vsWindow = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:VSWindow")
  );

  if (!playerWindow || !enemyWindow1 || !vsWindow)
    return;

  ICoord2D vsSize;
  vsWindow->winGetSize(&vsSize.x, &vsSize.y);

  Real scaleX = (Real)vsSize.x / 164.0f;
  Real scaleY = (Real)vsSize.y / 32.0f;

  Int playerBaseX = REAL_TO_INT_FLOOR(28.0f * scaleX);
  Int enemyBaseX = REAL_TO_INT_FLOOR(116.0f * scaleX);
  Int baseY = REAL_TO_INT_FLOOR(3.0f * scaleY);

  Int leftShift = REAL_TO_INT_FLOOR((35.0f / 2.4f) * scaleX);
  Int rightShift = REAL_TO_INT_FLOOR((29.0f / 2.4f) * scaleX);

  if (hasAlly && allyWindow)
  {
    playerWindow->winSetPosition(playerBaseX - leftShift, baseY);
    allyWindow->winSetPosition(playerBaseX + rightShift, baseY);
    allyWindow->winHide(FALSE);
  }
  else
  {
    playerWindow->winSetPosition(playerBaseX, baseY);

    if (allyWindow)
      allyWindow->winHide(TRUE);
  }

  if (hasSecondEnemy && enemyWindow2)
  {
    enemyWindow1->winSetPosition(enemyBaseX - leftShift, baseY);
    enemyWindow2->winSetPosition(enemyBaseX + rightShift, baseY);
    enemyWindow2->winHide(FALSE);
  }
  else
  {
    enemyWindow1->winSetPosition(enemyBaseX, baseY);

    if (enemyWindow2)
      enemyWindow2->winHide(TRUE);
  }
}

static void SetWindowVisible(const char* windowName, Bool visible)
{
  GameWindow* window = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey(windowName));

  if (window)
    window->winHide(!visible);
}

static void UpdateMissionFactionIcons(Int row)
{
  if (row < 0 || row >= (Int)campaignMissionData.size())
  {
    SetWindowImage("ChaptersMenu.wnd:PlayerFactionIcon", nullptr);
    SetWindowImage("ChaptersMenu.wnd:AllyFactionIcon1", nullptr);
    SetWindowImage("ChaptersMenu.wnd:EnemyFactionIcon1", nullptr);
    SetWindowImage("ChaptersMenu.wnd:EnemyFactionIcon2", nullptr);
    SetWindowVisible("ChaptersMenu.wnd:VsText", FALSE);
    PositionMissionFactionIcons(FALSE, FALSE);
    return;
  }

  const CampaignMissionInfo& mission = campaignMissionData[row];

  const Image* playerIcon = GetFactionIcon(mission.playerFaction);
  const Image* allyIcon1 = GetFactionIcon(mission.allyFaction1);
  const Image* enemyIcon1 = GetFactionIcon(mission.enemyFaction1);
  const Image* enemyIcon2 = GetFactionIcon(mission.enemyFaction2);

  DEBUG_LOG(("Faction icons: player='%s' ally1='%s' enemy1='%s' enemy2='%s' playerIcon=%p allyIcon1=%p enemyIcon1=%p enemyIcon2=%p\n",
    mission.playerFaction.c_str(),
    mission.allyFaction1.c_str(),
    mission.enemyFaction1.c_str(),
    mission.enemyFaction2.c_str(),
    playerIcon,
    allyIcon1,
    enemyIcon1,
    enemyIcon2));

  SetWindowImage("ChaptersMenu.wnd:PlayerFactionIcon", playerIcon);
  SetWindowImage("ChaptersMenu.wnd:AllyFactionIcon1", allyIcon1);
  SetWindowImage("ChaptersMenu.wnd:EnemyFactionIcon1", enemyIcon1);
  SetWindowImage("ChaptersMenu.wnd:EnemyFactionIcon2", enemyIcon2);

  PositionMissionFactionIcons(allyIcon1 != nullptr, enemyIcon2 != nullptr);

  SetWindowVisible("ChaptersMenu.wnd:VsText", playerIcon || allyIcon1 || enemyIcon1 || enemyIcon2);
}

static void NormalizeMapPath(AsciiString& path)
{
  std::string s = path.str();

  for (size_t i = 0; i < s.size(); ++i)
  {
    s[i] = (char)tolower((unsigned char)s[i]);

    if (s[i] == '/')
      s[i] = '\\';
  }

  path.set(s.c_str());
}

static Mission* FindMissionByMapPath(Campaign* campaign, const std::string& mapPath)
{
  if (!campaign)
    return nullptr;

  AsciiString wantedMap;
  wantedMap.set(mapPath.c_str());
  NormalizeMapPath(wantedMap);

  for (Campaign::MissionListIt it = campaign->m_missions.begin(); it != campaign->m_missions.end(); ++it)
  {
    Mission* mission = *it;

    if (!mission)
      continue;

    AsciiString missionMap = mission->m_mapName;
    NormalizeMapPath(missionMap);

    if (missionMap.compare(wantedMap) == 0)
      return mission;
  }

  return nullptr;
}

static void BringWorldMapLayersToTop()
{
  if (worldMapMarker)
    worldMapMarker->winBringToTop();

  GameWindow* worldMapBorder = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:WorldMapBorder")
  );

  if (worldMapBorder)
    worldMapBorder->winBringToTop();
}

static void UpdateMissionLocation(Int row)
{
  GameWindow* window = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:BattleLocationText")
  );

  if (!window)
    return;

  if (row < 0 || row >= (Int)campaignMissionData.size())
    return;

  Campaign* campaign = TheCampaignManager->getCurrentCampaign();
  Mission* mission = FindMissionByMapPath(campaign, campaignMissionData[row].mapPath);

  if (!mission)
  {
    GadgetStaticTextSetText(window, L"ERROR: Mission not found");
    return;
  }

  if (mission->m_locationNameLabelCM.isEmpty())
  {
    GadgetStaticTextSetText(window, L"ERROR: Undefined LocationNameLabelCM");
    return;
  }

  UnicodeString text = TheGameText->fetch(mission->m_locationNameLabelCM);

  static GameFont* font18 = nullptr;

  if (!font18)
    font18 = TheFontLibrary->getFont("Arial", 16, FALSE);

  window->winSetFont(font18);
  GadgetStaticTextSetText(window, text);
}

static void UpdateWorldMapMarkerPosition(Int row)
{
  if (!worldMapMarker)
    return;

  if (row < 0 || row >= (Int)campaignMissionData.size())
  {
    worldMapMarker->winHide(TRUE);
    return;
  }

  Campaign* campaign = TheCampaignManager->getCurrentCampaign();
  Mission* mission = FindMissionByMapPath(campaign, campaignMissionData[row].mapPath);

  if (!mission || mission->m_worldMapMarkerX < 0 || mission->m_worldMapMarkerY < 0)
  {
    worldMapMarker->winHide(TRUE);
    return;
  }

  GameWindow* worldMapBorder = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:WorldMapBorder")
  );

  if (!worldMapBorder)
  {
    worldMapMarker->winHide(TRUE);
    return;
  }

  Int borderX;
  Int borderY;
  worldMapBorder->winGetScreenPosition(&borderX, &borderY);

  ICoord2D borderSize;
  worldMapBorder->winGetSize(&borderSize.x, &borderSize.y);

  const Real designWorldMapWidth = 393.0f;
  const Real designWorldMapHeight = 250.0f;

  ICoord2D markerSize;
  worldMapMarker->winGetSize(&markerSize.x, &markerSize.y);

  Real scaleX = (Real)borderSize.x / designWorldMapWidth;
  Real scaleY = (Real)borderSize.y / designWorldMapHeight;

  Int markerX = REAL_TO_INT_FLOOR((Real)mission->m_worldMapMarkerX * scaleX);
  Int markerY = REAL_TO_INT_FLOOR((Real)mission->m_worldMapMarkerY * scaleY);

  worldMapMarker->winSetPosition(
    markerX - markerSize.x / 2,
    markerY - markerSize.y / 2
  );

worldMapMarker->winHide(FALSE);
BringWorldMapLayersToTop();
}

static void StartChapterMission(Int row, GameDifficulty diff)
{
  if (missionLaunchPending || startGame)
    return;

  if (row < 0 || row >= (Int)campaignMissionData.size())
    return;

  Campaign* campaign = TheCampaignManager->getCurrentCampaign();

  if (!campaign)
    return;

  const CampaignMissionInfo& info = campaignMissionData[row];

  AsciiString campaignName = campaign->m_name;
  AsciiString missionName;
  missionName.set(info.missionName.c_str());
  missionName.toLower();

  TheCampaignManager->setCampaignAndMission(campaignName, missionName);

  g_chapterMissionLaunchActive = TRUE;
  missionLaunchPending = TRUE;
  startGame = TRUE;

  TheCampaignManager->setGameDifficulty(diff);

  OptionPreferences pref;
  pref.setCampaignDifficulty(diff);
  pref.write();

  TheScriptEngine->setGlobalDifficulty(diff);

  TheWritableGlobalData->m_pendingFile = TheCampaignManager->getCurrentMap();

  TheShell->reverseAnimatewindow();
  TheTransitionHandler->setGroup("FadeWholeScreen");
}

static void ListboxClear(GameWindow* listbox)
{
  GadgetListBoxReset(listbox);
}

static void ListboxAdd(GameWindow* listbox, const char* text, Int row)
{
  UnicodeString utext;
  utext.translate(text);

  GadgetListBoxAddEntryText(listbox, utext, 0xFFFFFFFF, row);
}

static void DrawMapListboxDividers(GameWindow* window, WinInstanceData* instData)
{
  if (!window)
    return;

  NameKeyType listID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxMap");
  GameWindow* listbox = TheWindowManager->winGetWindowFromId(nullptr, listID);

  if (!listbox)
    return;

  Int winx;
  Int winy;
  listbox->winGetScreenPosition(&winx, &winy);

  ICoord2D size;
  listbox->winGetSize(&size.x, &size.y);

  const Int lineLeft = winx + 2;
  const Int lineRight = winx + size.x - 22;
  const Color lineColor = GameMakeColor(255, 255, 255, 55);

  Int lastDividerY = -1;

  // İlk iki divider'ı bulmak için
  Int firstDividerY = -1;
  Int secondDividerY = -1;

  for (Int y = 1; y < size.y; ++y)
  {
    Int rowA = -1;
    Int colA = -1;
    Int rowB = -1;
    Int colB = -1;

    GadgetListBoxGetEntryBasedOnXY(listbox, winx + 4, winy + y - 1, rowA, colA);
    GadgetListBoxGetEntryBasedOnXY(listbox, winx + 4, winy + y, rowB, colB);

    if (rowA >= 0 && rowB >= 0 && rowA != rowB)
    {
      Int drawY = winy + y + 4;

      // Çizgiyi çiz
      TheDisplay->drawLine(
        lineLeft,
        drawY,
        lineRight,
        drawY,
        1,
        lineColor
      );

      // Divider'ları kaydet
      if (firstDividerY == -1)
        firstDividerY = drawY;
      else if (secondDividerY == -1)
        secondDividerY = drawY;

      lastDividerY = drawY;
    }
  }

  // ✅ Ekstra çizgi (doğru hizalı)
  if (lastDividerY != -1 && firstDividerY != -1 && secondDividerY != -1)
  {
    Int rowHeight = secondDividerY - firstDividerY;

    TheDisplay->drawLine(
      lineLeft,
      lastDividerY + rowHeight,
      lineRight,
      lastDividerY + rowHeight,
      1,
      lineColor
    );
  }
}

static std::string TrimCopy(std::string text)
{
  while (!text.empty() && isspace((unsigned char)text.back()))
    text.pop_back();

  while (!text.empty() && isspace((unsigned char)text.front()))
    text.erase(0, 1);

  return text;
}

static void LoadCampaignMaps(const char* campaignName)
{
  campaignMissionData.clear();

  NameKeyType listID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxMap");
  GameWindow* listbox = TheWindowManager->winGetWindowFromId(nullptr, listID);

  if (!listbox)
    return;

  ListboxClear(listbox);

  File* file = TheFileSystem->openFile("RebornOmegaData\\Data\\INI\\Campaign.ini", File::READ | File::TEXT);

  if (!file)
    file = TheFileSystem->openFile("Data\\INI\\Campaign.ini", File::READ | File::TEXT);

  if (!file)
    return;

  std::string line;
  bool inCorrectCampaign = false;
  bool inMission = false;
  Int row = 0;
  std::string mapPath;
  std::string displayNameLabel;
  std::string missionName;
  std::string playerFaction;
  std::string allyFaction1;
  std::string enemyFaction1;
  std::string enemyFaction2;
  Int worldMapMarkerX = -1;
  Int worldMapMarkerY = -1;

  Char buffer[1024];

  while (!file->eof())
  {
    file->nextLine(buffer, sizeof(buffer));

    std::string line = buffer;
    std::string trimmed = TrimCopy(line);

    if (trimmed.empty() || trimmed[0] == ';')
      continue;

    if (trimmed.rfind("Campaign ", 0) == 0)
    {
      std::string foundName = TrimCopy(trimmed.substr(9));

      while (!foundName.empty() && isspace(foundName.back()))
        foundName.pop_back();

      while (!foundName.empty() && isspace(foundName.front()))
        foundName.erase(0, 1);

      inCorrectCampaign = (foundName == campaignName);
      inMission = false;
      mapPath.clear();
      displayNameLabel.clear();
      continue;
    }

    if (!inCorrectCampaign)
      continue;

    if (trimmed.rfind("Mission ", 0) == 0)
    {
      inMission = true;
      mapPath.clear();
      missionName = TrimCopy(trimmed.substr(8));
      displayNameLabel.clear();
      playerFaction.clear();
      allyFaction1.clear();
      enemyFaction1.clear();
      enemyFaction2.clear();
      worldMapMarkerX = -1;
      worldMapMarkerY = -1;
      continue;
    }

    if(trimmed == "END")
    {
      if (inMission)
      {
        if (!mapPath.empty())
        {
          UnicodeString utext;

          if (!displayNameLabel.empty())
            utext = TheGameText->fetch(displayNameLabel.c_str());
          else
            utext.translate(mapPath.c_str());

          GadgetListBoxAddEntryText(listbox, utext, 0xFFFFFFFF, row);

          CampaignMissionInfo info;
          info.mapPath = mapPath;
          info.displayNameLabel = displayNameLabel;
          info.missionName = missionName;
          info.playerFaction = playerFaction;
          info.allyFaction1 = allyFaction1;
          info.enemyFaction1 = enemyFaction1;
          info.enemyFaction2 = enemyFaction2;
          info.worldMapMarkerX = worldMapMarkerX;
          info.worldMapMarkerY = worldMapMarkerY;

          campaignMissionData.push_back(info);
          GadgetListBoxSetItemData(listbox, (void*)campaignMissionData.back().mapPath.c_str(), row);

          ChapterProgress progress;

          AsciiString campaign;
          campaign.set(campaignName);
          campaign.toLower();

          Int missionNumber = row + 1;
          Int completedDifficulty = progress.getMissionCompletedDifficulty(campaign, missionNumber);

          if (row > 0)
          {
            Int previousMissionNumber = row;

            if (progress.getMissionCompletedDifficulty(campaign, previousMissionNumber) <= 0)
              GadgetListBoxSetItemEnabled(listbox, row, FALSE);
          }

          const Image* completedImage = nullptr;

          if (completedDifficulty == 1)
            completedImage = TheMappedImageCollection->findImageByName("Star-Silver");
          else if (completedDifficulty == 2)
            completedImage = TheMappedImageCollection->findImageByName("Star-Gold");
          else if (completedDifficulty >= 3)
            completedImage = TheMappedImageCollection->findImageByName("RedYell_Star"); 

          if (completedImage)
          {
            Real scaleY = (Real)TheDisplay->getHeight() / 1080.0f;
            Int starSize = REAL_TO_INT_FLOOR(40.0f * scaleY);

            GadgetListBoxAddEntryImage(listbox, completedImage, row, 1, starSize, starSize);
            GadgetListBoxSetItemData(listbox, (void*)completedDifficulty, row, 1);
          }
          else
          {
            GadgetListBoxSetItemData(listbox, (void*)0, row, 1);
          }

          row++;
        }

        inMission = false;
        mapPath.clear();
        displayNameLabel.clear();
        playerFaction.clear();
        allyFaction1.clear();
        enemyFaction1.clear();
        enemyFaction2.clear();
        worldMapMarkerX = -1;
        worldMapMarkerY = -1;
      }
      else
      {
        break;
      }

      continue;
    }

    if (!inMission)
      continue;

    if (trimmed.rfind("Map ", 0) == 0)
    {
      mapPath = TrimCopy(trimmed.substr(4));

      while (!mapPath.empty() && isspace(mapPath.back()))
        mapPath.pop_back();

      while (!mapPath.empty() && isspace(mapPath.front()))
        mapPath.erase(0, 1);

      continue;
    }

    if (trimmed.rfind("DisplayName ", 0) == 0)
    {
      displayNameLabel = TrimCopy(trimmed.substr(12));

      while (!displayNameLabel.empty() && isspace(displayNameLabel.back()))
        displayNameLabel.pop_back();

      while (!displayNameLabel.empty() && isspace(displayNameLabel.front()))
        displayNameLabel.erase(0, 1);

      continue;
    }

    if (trimmed.rfind("PlayerFaction ", 0) == 0)
    {
      playerFaction = TrimCopy(trimmed.substr(14));
      continue;
    }

    if (trimmed.rfind("AllyFaction1 ", 0) == 0)
    {
      allyFaction1 = TrimCopy(trimmed.substr(13));
      continue;
    }

    if (trimmed.rfind("EnemyFaction1 ", 0) == 0)
    {
      enemyFaction1 = TrimCopy(trimmed.substr(14));
      continue;
    }

    if (trimmed.rfind("EnemyFaction2 ", 0) == 0)
    {
      enemyFaction2 = TrimCopy(trimmed.substr(14));
      continue;
    }

    if (trimmed.rfind("WorldMapMarkerX ", 0) == 0)
    {
      worldMapMarkerX = atoi(TrimCopy(trimmed.substr(16)).c_str());
      continue;
    }

    if (trimmed.rfind("WorldMapMarkerY ", 0) == 0)
    {
      worldMapMarkerY = atoi(TrimCopy(trimmed.substr(16)).c_str());
      continue;
    }

  }

  if (row > 0)
  {
    Int rowToSelect = lastSelectedMapRow;

    if (rowToSelect < 0 || rowToSelect >= row)
      rowToSelect = 0;

    if (!GadgetListBoxIsItemEnabled(listbox, rowToSelect))
      rowToSelect = 0;

    GadgetListBoxSetSelected(listbox, rowToSelect);
    TheWindowManager->winSetFocus(listbox);

    ignoreNextMapSelectionForDoubleClick = TRUE;

    TheWindowManager->winSendSystemMsg(
      listbox,
      GLM_SELECTED,
      (WindowMsgData)listbox,
      rowToSelect
    );
  }

  file->close();
}

static void SelectChapter(Int index)
{
  if (selectedChapter != index)
    lastSelectedMapRow = 0;

  DEBUG_LOG(("SelectChapter(%d)\n", index));

  selectedChapter = index;

  for (Int i = 0; i < 8; ++i)
  {
    DEBUG_LOG(("Reset %d\n", i));

    if (!chapterButtons[i])
      continue;

    if (!chapterButtons[i]->winGetEnabled())
      continue;

    if (i == index)
    {
      chapterButtons[i]->winSetEnabledImage(0, selectedImages[i][0]);
      chapterButtons[i]->winSetEnabledImage(5, selectedImages[i][1]);
      chapterButtons[i]->winSetEnabledImage(6, selectedImages[i][2]);

      chapterButtons[i]->winSetHiliteImage(0, selectedImages[i][0]);
      chapterButtons[i]->winSetHiliteImage(5, selectedImages[i][1]);
      chapterButtons[i]->winSetHiliteImage(6, selectedImages[i][2]);

      chapterButtons[i]->winSetEnabledTextColors(
        selectedTextColor[i],
        selectedTextBorderColor[i]
      );

      chapterButtons[i]->winSetHiliteTextColors(
        selectedTextColor[i],
        selectedTextBorderColor[i]
      );
    }
    else
    {
      chapterButtons[i]->winSetEnabledImage(0, normalImages[i][0]);
      chapterButtons[i]->winSetEnabledImage(5, normalImages[i][1]);
      chapterButtons[i]->winSetEnabledImage(6, normalImages[i][2]);

      chapterButtons[i]->winSetHiliteImage(0, normalHiliteImages[i][0]);
      chapterButtons[i]->winSetHiliteImage(5, normalHiliteImages[i][1]);
      chapterButtons[i]->winSetHiliteImage(6, normalHiliteImages[i][2]);

      chapterButtons[i]->winSetEnabledTextColors(
        normalTextColor[i],
        normalTextBorderColor[i]
      );

      chapterButtons[i]->winSetHiliteTextColors(
        normalHiliteTextColor[i],
        normalHiliteTextBorderColor[i]
      );
    }
  }


	// Reborn: Fill map listbox with maps for the selected chapter
  if (index == 0) // TRAINING
  {
    TheCampaignManager->setCampaign("TRAINING");
    LoadCampaignMaps("TRAINING");
  }
  else if (index == 2) // ChinaGen
  {
    TheCampaignManager->setCampaign("China_Gen");
    LoadCampaignMaps("China_Gen");
  }
  else if (index == 4) // GLAGen
  {
    TheCampaignManager->setCampaign("GLA_Gen");
    LoadCampaignMaps("GLA_Gen");
  }


}

WindowMsgHandledType ChaptersMenuDividerInput(GameWindow* window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
  return MSG_IGNORED;
}

static void DrawWorldMapMarkerClipped(GameWindow* window, WinInstanceData* instData)
{
  ICoord2D origin;
  ICoord2D size;

  window->winGetScreenPosition(&origin.x, &origin.y);
  window->winGetSize(&size.x, &size.y);

  const Image* image;

  if (!BitIsSet(window->winGetStatus(), WIN_STATUS_ENABLED))
    image = window->winGetDisabledImage(0);
  else if (BitIsSet(instData->getState(), WIN_STATE_HILITED))
    image = window->winGetHiliteImage(0);
  else
    image = window->winGetEnabledImage(0);

  if (!image)
    return;

  GameWindow* worldMap = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:WorldMap")
  );

  if (!worldMap)
    return;

  ICoord2D mapOrigin;
  ICoord2D mapSize;

  worldMap->winGetScreenPosition(&mapOrigin.x, &mapOrigin.y);
  worldMap->winGetSize(&mapSize.x, &mapSize.y);

  const Real scaleX = (Real)mapSize.x / 393.0f;
  const Real scaleY = (Real)mapSize.y / 250.0f;

  const Int insetX = REAL_TO_INT_FLOOR(2.0f * scaleX) < 1 ? 1 : REAL_TO_INT_FLOOR(2.0f * scaleX);
  const Int insetY = REAL_TO_INT_FLOOR(2.0f * scaleY) < 1 ? 1 : REAL_TO_INT_FLOOR(2.0f * scaleY);

  IRegion2D clipRegion;
  clipRegion.lo.x = mapOrigin.x + insetX;
  clipRegion.lo.y = mapOrigin.y + insetY;
  clipRegion.hi.x = mapOrigin.x + mapSize.x - insetX;
  clipRegion.hi.y = mapOrigin.y + mapSize.y - insetY;

  TheDisplay->setClipRegion(&clipRegion);

  Int startX = origin.x + instData->m_imageOffset.x;
  Int startY = origin.y + instData->m_imageOffset.y;

  TheDisplay->drawImage(
    image,
    startX,
    startY,
    startX + size.x,
    startY + size.y
  );

  IRegion2D fullScreenRegion;
  fullScreenRegion.lo.x = 0;
  fullScreenRegion.lo.y = 0;
  fullScreenRegion.hi.x = TheDisplay->getWidth();
  fullScreenRegion.hi.y = TheDisplay->getHeight();

  TheDisplay->setClipRegion(&fullScreenRegion);
}

void ChaptersMenuInit(WindowLayout* layout, void* userData)
{

  const Bool useGeneralsLayout = layout &&
    layout->getFilename().compareNoCase("Menus/ChaptersMenuGen.wnd") == 0;
  if (useGeneralsLayout != s_chaptersMenuUsesGeneralsLayout)
  {
    // Reborn: Discard button-image pointers cached from the other Chapters visual theme.
    memset(chapterButtons, 0, sizeof(chapterButtons));
    chapterImagesCached = FALSE;
  }
  s_chaptersMenuUsesGeneralsLayout = useGeneralsLayout; // Reborn: Derive child IDs from the layout selected by Main Menu.

  missionLaunchPending = FALSE;
  startGame = FALSE;

  TheSupplyAndTechImageLocations.m_supplyPosList.clear();
  TheSupplyAndTechImageLocations.m_techPosList.clear();


  if (resetChapterImagesOnNextInit)
  {
		memset(chapterButtons, 0, sizeof(chapterButtons));
    chapterImagesCached = FALSE;
    resetChapterImagesOnNextInit = FALSE;
	}

  if (selectedChapter < 0)
  {
    memset(chapterButtons, 0, sizeof(chapterButtons));
  }

  TheShell->showShellMap(TRUE);

  radioEasyID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:RadioButtonEasyAI");
  radioMediumID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:RadioButtonMediumAI");
  radioHardID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:RadioButtonHardAI");

  PositionDifficultyStars();

  buttonBackID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ButtonBack");
  buttonStartID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ButtonStart");
  buttonPushed = FALSE;

  isShuttingDown = FALSE;

  OptionPreferences pref;
  selectedDifficulty = (GameDifficulty)pref.getCampaignDifficulty();

  NameKeyType selectedRadioID = radioMediumID;

  if (selectedDifficulty == DIFFICULTY_EASY)
    selectedRadioID = radioEasyID;
  else if (selectedDifficulty == DIFFICULTY_HARD)
    selectedRadioID = radioHardID;

  GameWindow* radio = TheWindowManager->winGetWindowFromId(nullptr, selectedRadioID);
  if (radio)
    TheWindowManager->winSendSystemMsg(radio, GBM_SET_SELECTION, TRUE, 0);

  layout->hide(FALSE);
  layout->bringForward();

  GameWindow* subParent = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:SubParent")
  );

  if (subParent)
  {
    subParent->winHide(TRUE);
  }

  justEntered = TRUE;
  initialGadgetDelay = 2;

  //NameKeyType listID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxMap");
  //GameWindow* listbox = TheWindowManager->winGetWindowFromId(nullptr, listID);

  NameKeyType dividerID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxDividerOverlay");
  GameWindow* divider = TheWindowManager->winGetWindowFromId(nullptr, dividerID);

  if (divider)
  {
    divider->winEnable(FALSE);
    divider->winHide(FALSE);
    divider->winSetDrawFunc(DrawMapListboxDividers);
  }

  if (TheMapCache)
    TheMapCache->updateCache();

  for (Int i = 0; i < 8; ++i)
  {
    AsciiString name;
    name.format("ChaptersMenu.wnd:ButtonMapStartPosition%d", i);

    GameWindow* button = TheWindowManager->winGetWindowFromId(
      nullptr,
      GetChaptersMenuWindowKey(name.str()));

    if (button)
      button->winHide(TRUE);
  }

  const char* names[8] =
  {
      "ChaptersMenu.wnd:ButtonTRAINING",   // 0
      "ChaptersMenu.wnd:ButtonChallenge",  // 1
      "ChaptersMenu.wnd:ButtonChinaGen",   // 2
      "ChaptersMenu.wnd:ButtonChina",      // 3
      "ChaptersMenu.wnd:ButtonGLAGen",     // 4
      "ChaptersMenu.wnd:ButtonGLA",        // 5
      "ChaptersMenu.wnd:ButtonUSAGen",     // 6
      "ChaptersMenu.wnd:ButtonUSA"         // 7
  };


  Int chapterToSelect = selectedChapter;
  if (chapterToSelect < 0)
    chapterToSelect = 0;

  for (Int i = 0; i < 8; ++i)
  {
    chapterButtons[i] = TheWindowManager->winGetWindowFromId(
      nullptr,
      GetChaptersMenuWindowKey(names[i]));

    if (chapterButtons[i])
    {
      if (!chapterImagesCached)
      {
        normalImages[i][0] = chapterButtons[i]->winGetEnabledImage(0);
        normalImages[i][1] = chapterButtons[i]->winGetEnabledImage(5);
        normalImages[i][2] = chapterButtons[i]->winGetEnabledImage(6);

        normalHiliteImages[i][0] = chapterButtons[i]->winGetHiliteImage(0);
        normalHiliteImages[i][1] = chapterButtons[i]->winGetHiliteImage(5);
        normalHiliteImages[i][2] = chapterButtons[i]->winGetHiliteImage(6);

        selectedImages[i][0] = chapterButtons[i]->winGetHiliteImage(1);
        selectedImages[i][1] = chapterButtons[i]->winGetHiliteImage(3);
        selectedImages[i][2] = chapterButtons[i]->winGetHiliteImage(4);

        normalTextColor[i] = chapterButtons[i]->winGetEnabledTextColor();
        normalTextBorderColor[i] = chapterButtons[i]->winGetEnabledTextBorderColor();

        normalHiliteTextColor[i] = chapterButtons[i]->winGetHiliteTextColor();
        normalHiliteTextBorderColor[i] = chapterButtons[i]->winGetHiliteTextBorderColor();

        selectedTextColor[i] = chapterButtons[i]->winGetHiliteTextColor();
        selectedTextBorderColor[i] = chapterButtons[i]->winGetHiliteTextBorderColor();
      }


      // Reborn: Only Training (0), ChinaGen (2) and GLAGen (4) will be active atm.
      if (i == 0 || i == 2 || i == 4)
        chapterButtons[i]->winEnable(TRUE);
      else
        chapterButtons[i]->winEnable(FALSE);

    }
  }
  chapterImagesCached = TRUE;

  worldMapMarker = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:WorldMapMarker")
  );

  if (worldMapMarker)
  {
    for (Int i = 0; i < 20; ++i)
    {
      AsciiString imageName;
      imageName.format("SCCSniper_WorldMapMarker_%02d", i);
      worldMapMarkerImages[i] = TheMappedImageCollection->findImageByName(imageName.str());
    }

    worldMapMarkerFrame = 0;
    worldMapMarkerLastFrameTime = timeGetTime();
    worldMapMarker->winSetStatus(WIN_STATUS_IMAGE);
    worldMapMarker->winSetEnabledImage(0, worldMapMarkerImages[0]);
    worldMapMarker->winSetDrawFunc(DrawWorldMapMarkerClipped);
    worldMapMarker->winHide(FALSE);
    BringWorldMapLayersToTop();
  }

  SelectChapter(chapterToSelect);

  //TheTransitionHandler->setGroup(GetChaptersMenuTransitionGroup());
}

static void shutdownComplete(WindowLayout* layout)
{
  layout->hide(TRUE);

  GameWindow* preview = TheWindowManager->winGetWindowFromId(
    nullptr,
    GetChaptersMenuWindowKey("ChaptersMenu.wnd:MapWindow"));

  if (preview)
    preview->winSetUserData(nullptr);

  TheSupplyAndTechImageLocations.m_supplyPosList.clear();
  TheSupplyAndTechImageLocations.m_techPosList.clear();

  TheShell->shutdownComplete(layout);
}

void ChaptersMenuShutdown(WindowLayout* layout, void* userData)
{
  Bool popImmediate = userData
    ? *static_cast<Bool*>(userData)
    : FALSE;

  if (popImmediate)
  {
    shutdownComplete(layout);
    return;
  }

  isShuttingDown = TRUE;
  TheShell->reverseAnimatewindow();
  TheTransitionHandler->reverse(GetChaptersMenuTransitionGroup());
}

void ChaptersMenuUpdate(WindowLayout* layout, void* userData)
{

  if (worldMapMarker && worldMapMarkerImages[0])
  {
    UnsignedInt now = timeGetTime();

    if (now - worldMapMarkerLastFrameTime >= 50)
    {
      worldMapMarkerLastFrameTime = now;
      worldMapMarkerFrame = (worldMapMarkerFrame + 1) % 20;

      if (worldMapMarkerImages[worldMapMarkerFrame])
        worldMapMarker->winSetEnabledImage(0, worldMapMarkerImages[worldMapMarkerFrame]);
    }
  }

  if (justEntered)
  {
    if (initialGadgetDelay == 1)
    {
      GameWindow* subParent = TheWindowManager->winGetWindowFromId(
        nullptr,
        GetChaptersMenuWindowKey("ChaptersMenu.wnd:SubParent")
      );

      if (subParent)
        subParent->winHide(FALSE);

      TheTransitionHandler->setGroup(GetChaptersMenuTransitionGroup());
      initialGadgetDelay = 2;
      justEntered = FALSE;
    }
    else
    {
      initialGadgetDelay--;
    }
  }

  if (startGame && TheShell->isAnimFinished() && TheTransitionHandler->isFinished())
  {

    DEBUG_LOG(("Appending MSG_NEW_GAME from ChaptersMenuUpdate startGame=%d inGame=%d loadingMap=%d",
      startGame,
      TheGameLogic->isInGame(),
      TheGameLogic->isLoadingMap()));

    startGame = FALSE;

    if (TheGameLogic->isInGame())
      TheGameLogic->clearGameData();

    GameMessage* msg = TheMessageStream->appendMessage(GameMessage::MSG_NEW_GAME);
    msg->appendIntegerArgument(GAME_SINGLE_PLAYER);
    msg->appendIntegerArgument(TheCampaignManager->getGameDifficulty());
    msg->appendIntegerArgument(TheCampaignManager->getRankPoints());
    InitRandom(0);

    TheShell->shutdownComplete(layout);
    return;
  }

  if (isShuttingDown && TheShell->isAnimFinished() && TheTransitionHandler->isFinished())
    shutdownComplete(layout);
}

static Bool LaunchSelectedChapterMission(GameWindow* listbox)
{
  if (!listbox)
    return FALSE;

  Int selected = -1;
  GadgetListBoxGetSelected(listbox, &selected);

  if (selected < 0)
    return FALSE;

  if (!GadgetListBoxIsItemEnabled(listbox, selected))
    return FALSE;

  const char* mapFname = (const char*)GadgetListBoxGetItemData(listbox, selected);

  if (!mapFname)
    return FALSE;

  StartChapterMission(selected, selectedDifficulty);
  return TRUE;
}

static void PopulateSupplyAndTechIcons(const MapMetaData* mmd, GameWindow* mapWindow)
{
  TheSupplyAndTechImageLocations.m_supplyPosList.clear();
  TheSupplyAndTechImageLocations.m_techPosList.clear();

  if (!mmd || !mapWindow)
    return;

  ICoord2D winMapSize;
  mapWindow->winGetSize(&winMapSize.x, &winMapSize.y);

  ICoord2D ul, lr;
  findDrawPositions(
    0,
    0,
    winMapSize.x,
    winMapSize.y,
    mmd->m_extent,
    &ul,
    &lr
  );

  Int smallWidth = lr.x - ul.x;
  Int smallHeight = lr.y - ul.y;

  for (Coord3DList::const_iterator it = mmd->m_supplyPositions.begin();
    it != mmd->m_supplyPositions.end();
    ++it)
  {
    ICoord2D markerPos;

    Real pos =
      (it->x - mmd->m_extent.lo.x) /
      (mmd->m_extent.hi.x - mmd->m_extent.lo.x);

    markerPos.x =
      (pos * smallWidth) - SUPPLY_TECH_SIZE / 2 + ul.x;

    pos =
      (it->y - mmd->m_extent.lo.y) /
      (mmd->m_extent.hi.y - mmd->m_extent.lo.y);

    markerPos.y =
      ((1 - pos) * smallHeight) - SUPPLY_TECH_SIZE / 2 + ul.y;

    TheSupplyAndTechImageLocations.m_supplyPosList.push_front(markerPos);
  }

  for (Coord3DList::const_iterator it = mmd->m_techPositions.begin();
    it != mmd->m_techPositions.end();
    ++it)
  {
    ICoord2D markerPos;

    Real pos =
      (it->x - mmd->m_extent.lo.x) /
      (mmd->m_extent.hi.x - mmd->m_extent.lo.x);

    markerPos.x =
      (pos * smallWidth) - SUPPLY_TECH_SIZE / 2 + ul.x;

    pos =
      (it->y - mmd->m_extent.lo.y) /
      (mmd->m_extent.hi.y - mmd->m_extent.lo.y);

    markerPos.y =
      ((1 - pos) * smallHeight) - SUPPLY_TECH_SIZE / 2 + ul.y;

    TheSupplyAndTechImageLocations.m_techPosList.push_front(markerPos);
  }
}

WindowMsgHandledType ChaptersMenuSystem(GameWindow* window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
  DEBUG_LOG(("ChaptersMenuSystem: msg=%u\n", msg));

  if (missionLaunchPending)
    return MSG_HANDLED;

  switch (msg)
  {
  case GBM_SELECTED:
  {
    GameWindow* control = (GameWindow*)mData1;

    if (!control)
      break;

    for (Int i = 0; i < 8; ++i)
    {
      if (control == chapterButtons[i])
      {
        SelectChapter(i);
        return MSG_HANDLED;
      }
    }

    Int controlID = control->winGetWindowId();

    if (buttonPushed)
      break;

    if (controlID == buttonStartID)
    {
      NameKeyType listID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxMap");
      GameWindow* listbox = TheWindowManager->winGetWindowFromId(nullptr, listID);

      LaunchSelectedChapterMission(listbox);
      return MSG_HANDLED;
    }
    if (controlID == buttonBackID)
    {
      buttonPushed = TRUE;
			resetChapterImagesOnNextInit = TRUE;
      resetDifficultyOnNextInit = TRUE;

			g_chapterMissionLaunchActive = FALSE;
      TheCampaignManager->setCampaign(AsciiString::TheEmptyString);

      TheShell->pop();
      return MSG_HANDLED;
    }
    if (controlID == radioEasyID)
    {
      selectedDifficulty = DIFFICULTY_EASY;
      OptionPreferences pref;
      pref.setCampaignDifficulty(selectedDifficulty);
      pref.write();
      return MSG_HANDLED;
    }
    else if (controlID == radioMediumID)
    {
      selectedDifficulty = DIFFICULTY_NORMAL;
      OptionPreferences pref;
      pref.setCampaignDifficulty(selectedDifficulty);
      pref.write();
      return MSG_HANDLED;
    }
    else if (controlID == radioHardID)
    {
      selectedDifficulty = DIFFICULTY_HARD;
      OptionPreferences pref;
      pref.setCampaignDifficulty(selectedDifficulty);
      pref.write();
      return MSG_HANDLED;
    }

    break;
  }

  case GLM_SELECTED:
  {
    GameWindow* control = (GameWindow*)mData1;
    if (!control)
      break;

    NameKeyType listID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxMap");

    if (control->winGetWindowId() == listID)
    {
      int rowSelected = mData2;

      if (rowSelected < 0)
      {
        GadgetListBoxSetSelected(control, lastSelectedMapRow);
        return MSG_HANDLED;
      }

      if (!GadgetListBoxIsItemEnabled(control, rowSelected))
      {
        GadgetListBoxSetSelected(control, lastSelectedMapRow);
        return MSG_HANDLED;
      }

      if (ignoreNextMapSelectionForDoubleClick)
      {
        ignoreNextMapSelectionForDoubleClick = FALSE;
      }

      lastMapClickTime = 0;
      lastMapClickRow = -1;

      lastSelectedMapRow = rowSelected;

      GameWindow* listbox = control;

      const char* mapFname = (const char*)GadgetListBoxGetItemData(listbox, rowSelected);

      if (!mapFname)
        break;

      std::string fixedPath = mapFname;

      for (size_t i = 0; i < fixedPath.size(); ++i)
      {
        fixedPath[i] = (char)tolower(fixedPath[i]);
      }

      AsciiString asciiMap = fixedPath.c_str();
      asciiMap.toLower();

      NameKeyType previewID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:MapWindow");
      GameWindow* preview = TheWindowManager->winGetWindowFromId(nullptr, previewID);

      if (!preview)
        break;

      preview->winSetStatus(WIN_STATUS_IMAGE);


      TheSupplyAndTechImageLocations.m_supplyPosList.clear();
      TheSupplyAndTechImageLocations.m_techPosList.clear();

      Image* image = getMapPreviewImage(asciiMap);
      preview->winSetUserData((void*)TheMapCache->findMap(asciiMap));

      const MapMetaData* md = TheMapCache->findMap(asciiMap);
      PopulateSupplyAndTechIcons(md, preview);

      if (image)
      {
        preview->winSetEnabledImage(0, image);
      }
      else
      {
        preview->winClearStatus(WIN_STATUS_IMAGE);
      }

      UpdateMissionFactionIcons(rowSelected);
      UpdateMissionLocation(rowSelected);
      UpdateWorldMapMarkerPosition(rowSelected);

    }

    break;
  }


  case GLM_DOUBLE_CLICKED:
  {
    DEBUG_LOG(("ChaptersMenuSystem: GLM_DOUBLE_CLICKED mData2=%d\n", (Int)mData2));

    GameWindow* listbox = (GameWindow*)mData1;

    if (!listbox)
      return MSG_HANDLED;

    NameKeyType listID = GetChaptersMenuWindowKey("ChaptersMenu.wnd:ListboxMap");

    if (listbox->winGetWindowId() != listID)
      return MSG_HANDLED;

    Int selected = (Int)mData2;

    if (selected < 0)
      GadgetListBoxGetSelected(listbox, &selected);

    if (selected < 0)
      return MSG_HANDLED;

    if (!GadgetListBoxIsItemEnabled(listbox, selected))
      return MSG_HANDLED;

    const char* mapFname = (const char*)GadgetListBoxGetItemData(listbox, selected);

    if (!mapFname)
      return MSG_HANDLED;

    GadgetListBoxSetSelected(listbox, selected);

    StartChapterMission(selected, selectedDifficulty);

    return MSG_HANDLED;
  }


  }

  return MSG_IGNORED;
}

WindowMsgHandledType ChaptersMenuInput(GameWindow* window, UnsignedInt msg, WindowMsgData mData1, WindowMsgData mData2)
{
  if (msg == GWM_LEFT_UP)
  {
    GameWindow* easyLeftStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:EasyLeftStar"));
    GameWindow* easyRightStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:EasyRightStar"));
    GameWindow* mediumLeftStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:MediumLeftStar"));
    GameWindow* mediumRightStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:MediumRightStar"));
    GameWindow* hardLeftStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:HardLeftStar"));
    GameWindow* hardRightStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:HardRightStar"));

    GameWindow* radio = nullptr;

    if (window == easyLeftStar || window == easyRightStar)
    {
      selectedDifficulty = DIFFICULTY_EASY;
      OptionPreferences pref;
      pref.setCampaignDifficulty(selectedDifficulty);
      pref.write();
      radio = TheWindowManager->winGetWindowFromId(nullptr, radioEasyID);
    }
    else if (window == mediumLeftStar || window == mediumRightStar)
    {
      selectedDifficulty = DIFFICULTY_NORMAL;
      OptionPreferences pref;
      pref.setCampaignDifficulty(selectedDifficulty);
      pref.write();
      radio = TheWindowManager->winGetWindowFromId(nullptr, radioMediumID);
    }
    else if (window == hardLeftStar || window == hardRightStar)
    {
      selectedDifficulty = DIFFICULTY_HARD;
      OptionPreferences pref;
      pref.setCampaignDifficulty(selectedDifficulty);
      pref.write();
      radio = TheWindowManager->winGetWindowFromId(nullptr, radioHardID);
    }

    if (radio)
    {
      TheWindowManager->winSendSystemMsg(radio, GBM_SET_SELECTION, TRUE, 0);
      return MSG_HANDLED;
    }
  }

  if (msg == GWM_MOUSE_ENTERING || msg == GWM_MOUSE_LEAVING)
  {
    GameWindow* easyLeftStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:EasyLeftStar"));
    GameWindow* easyRightStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:EasyRightStar"));
    GameWindow* mediumLeftStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:MediumLeftStar"));
    GameWindow* mediumRightStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:MediumRightStar"));
    GameWindow* hardLeftStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:HardLeftStar"));
    GameWindow* hardRightStar = TheWindowManager->winGetWindowFromId(nullptr, GetChaptersMenuWindowKey("ChaptersMenu.wnd:HardRightStar"));

    GameWindow* radio = nullptr;

    if (window == easyLeftStar || window == easyRightStar)
      radio = TheWindowManager->winGetWindowFromId(nullptr, radioEasyID);
    else if (window == mediumLeftStar || window == mediumRightStar)
      radio = TheWindowManager->winGetWindowFromId(nullptr, radioMediumID);
    else if (window == hardLeftStar || window == hardRightStar)
      radio = TheWindowManager->winGetWindowFromId(nullptr, radioHardID);

    if (radio)
    {
      radio->winSetHiliteState(msg == GWM_MOUSE_ENTERING);
      return MSG_HANDLED;
    }
  }

#ifdef RTS_DEBUG
  if (msg == GWM_LEFT_UP)
  {
    Int screenX = (Int)(mData1 & 0xFFFF);
    Int screenY = (Int)((mData1 >> 16) & 0xFFFF);

    GameWindow* worldMapBorder = TheWindowManager->winGetWindowFromId(
      nullptr,
      GetChaptersMenuWindowKey("ChaptersMenu.wnd:WorldMapBorder")
    );

    if (worldMapBorder)
    {
      Int wx, wy;
      worldMapBorder->winGetScreenPosition(&wx, &wy);

      ICoord2D size;
      worldMapBorder->winGetSize(&size.x, &size.y);

      DEBUG_LOG(("WorldMapMarker: X=%d Y=%d BorderW=%d BorderH=%d\n",
        screenX - wx,
        screenY - wy,
        size.x,
        size.y));
    }
  }
#endif

  return MSG_IGNORED;
}
