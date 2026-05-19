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

// FILE: ControlBarPopupDescription.cpp /////////////////////////////////////////////////
//-----------------------------------------------------------------------------
//
//                       Electronic Arts Pacific.
//
//                       Confidential Information
//                Copyright (C) 2002 - All Rights Reserved
//
//-----------------------------------------------------------------------------
//
//	created:	Sep 2002
//
//	Filename: 	ControlBarPopupDescription.cpp
//
//	author:		Chris Huybregts
//
//	purpose:
//
//-----------------------------------------------------------------------------
///////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------------------
// SYSTEM INCLUDES ////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// USER INCLUDES //////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// DEFINES ////////////////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PUBLIC FUNCTIONS ///////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PRIVATE FUNCTIONS //////////////////////////////////////////////////////////
//-----------------------------------------------------------------------------


// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/GlobalData.h"
#include "Common/BuildAssistant.h"
#include "Common/Player.h"
#include "Common/PlayerList.h"
#include "Common/ProductionPrerequisite.h"
#include "Common/SpecialPower.h"
#include "Common/ThingTemplate.h"
#include "Common/Upgrade.h"
#include "Common/ThingFactory.h"
#include "GameClient/AnimateWindowManager.h"
#include "GameClient/DisconnectMenu.h"
#include "GameClient/GameWindow.h"
#include "GameClient/Gadget.h"
#include "GameClient/GadgetTextEntry.h"
#include "GameClient/GadgetPushButton.h"
#include "GameClient/GadgetStaticText.h"
#include "GameClient/GameClient.h"
#include "GameClient/GameText.h"
#include "GameClient/GUICallbacks.h"
#include "GameClient/InGameUI.h"
#include "GameClient/ControlBar.h"
#include "GameClient/DisplayStringManager.h"
#include "GameLogic/Module/BattlePlanUpdate.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Module/MaxHealthUpgrade.h"
#include "GameLogic/Module/OverchargeBehavior.h"
#include "GameLogic/Module/ProductionUpdate.h"
#include "GameLogic/ScriptEngine.h"
#include "GameLogic/Module/ActiveBody.h"
#include "GameLogic/Module/AIUpdate.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Object.h"
#include "GameLogic/LocomotorSet.h"
#include "GameLogic/Module/AutoDepositUpdate.h"
#include "GameLogic/Module/HackInternetAIUpdate.h"
#include "GameLogic/Module/InternetHackContain.h"
#include "GameLogic/Module/LocomotorSetUpgrade.h"
#include "GameLogic/Module/MoneyCrateCollide.h"
#include "GameLogic/Module/OCLUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Module/PowerPlantUpgrade.h"
#include "GameLogic/Module/SpecialPowerModule.h"
#include "GameLogic/Module/StealthDetectorUpdate.h"
#include "GameLogic/Module/SpawnBehavior.h"
#include "GameLogic/Module/ObjectCreationUpgrade.h"
#include "GameLogic/Module/SupplyWarehouseDockUpdate.h"
#include "GameLogic/ObjectCreationList.h"


#include "GameNetwork/NetworkInterface.h"

static WindowLayout *theLayout = nullptr;
static GameWindow *theWindow = nullptr;
static AnimateWindowManager *theAnimateWindowManager = nullptr;
static GameWindow *prevWindow = nullptr;
static Bool useAnimation = FALSE;
static Bool s_tooltipSizesInitialized = FALSE;
static ICoord2D s_tooltipRootDefaultSize = { 0, 0 };
static ICoord2D s_tooltipRootDefaultPos = { 0, 0 };
static ICoord2D s_tooltipCostDefaultSize = { 0, 0 };
static ICoord2D s_tooltipDescDefaultSize = { 0, 0 };
static ICoord2D s_tooltipDescDefaultPos = { 0, 0 };
const ThingTemplate* thingTemplate = nullptr;

extern Int g_resourceMultiplierPercent;

void ControlBarPopupDescriptionUpdateFunc(WindowLayout* layout, void* param)
{
	if (TheScriptEngine->isGameEnding())
		TheControlBar->hideBuildTooltipLayout();

	if (theAnimateWindowManager && !TheControlBar->getShowBuildTooltipLayout() && !theAnimateWindowManager->isReversed())
		theAnimateWindowManager->reverseAnimateWindow();
	else if (!TheControlBar->getShowBuildTooltipLayout() && (!TheGlobalData->m_animateWindows || !useAnimation))
		TheControlBar->deleteBuildTooltipLayout();

	if (TheControlBar->getShowBuildTooltipLayout())
	{
		TheControlBar->repopulateBuildTooltipLayout();
	}

	if (useAnimation && theAnimateWindowManager && TheGlobalData->m_animateWindows)
	{
		Bool wasFinished = theAnimateWindowManager->isFinished();
		theAnimateWindowManager->update();
		if (theAnimateWindowManager->isFinished() && !wasFinished && theAnimateWindowManager->isReversed())
		{
			delete theAnimateWindowManager;
			theAnimateWindowManager = nullptr;
			TheControlBar->deleteBuildTooltipLayout();
		}
	}
}

// ---------------------------------------------------------------------------------------
void ControlBar::showBuildTooltipLayout(GameWindow* cmdButton)
{
	if ((TheGameLogic && TheGameLogic->isGamePaused()) || TheInGameUI->areTooltipsDisabled() || TheScriptEngine->isGameEnding())
	{
		hideBuildTooltipLayout();
		return;
	}

	Bool passedWaitTime = FALSE;
	static Bool isInitialized = FALSE;
	static UnsignedInt beginWaitTime;
	if(prevWindow == cmdButton)
	{
		m_showBuildToolTipLayout = TRUE;
		if(!isInitialized &&  beginWaitTime + cmdButton->getTooltipDelay() < timeGetTime())
		{
			//DEBUG_LOG(("%d beginwaittime, %d tooltipdelay, %dtimegettime", beginWaitTime, cmdButton->getTooltipDelay(), timeGetTime()));
			passedWaitTime = TRUE;
		}

		if(!passedWaitTime)
			return;
	}
	else if( !m_buildToolTipLayout->isHidden() )
	{
		if(useAnimation && TheGlobalData->m_animateWindows && !theAnimateWindowManager->isReversed())
			theAnimateWindowManager->reverseAnimateWindow();
		else if( useAnimation && TheGlobalData->m_animateWindows && theAnimateWindowManager->isReversed())
		{
			return;
		}
		else
		{
//			m_buildToolTipLayout->destroyWindows();
//			deleteInstance(m_buildToolTipLayout);
//			m_buildToolTipLayout = nullptr;
			m_buildToolTipLayout->hide(TRUE);
			prevWindow = nullptr;
		}
		return;
	}


	// will only get here the firsttime through the function through this window
	if(!passedWaitTime)
	{
		prevWindow = cmdButton;
		beginWaitTime = timeGetTime();
		isInitialized = FALSE;
		return;
	}
	isInitialized = TRUE;

	if(!cmdButton)
		return;
	if(BitIsSet(cmdButton->winGetStyle(), GWS_PUSH_BUTTON))
	{
		const CommandButton *commandButton = (const CommandButton *)GadgetButtonGetData(cmdButton);

		if(!commandButton)
			return;

		// note that, in this branch, ENABLE_SOLO_PLAY is ***NEVER*** defined...
		// this is so that we have a multiplayer build that cannot possibly be hacked
		// to work as a solo game!
		if (TheGameLogic->isInReplayGame())
			return;

		if (TheInGameUI->isQuitMenuVisible())
			return;

		if (TheDisconnectMenu && TheDisconnectMenu->isScreenVisible())
			return;

		//	if (m_buildToolTipLayout)
		//	{
		//		m_buildToolTipLayout->destroyWindows();
		//		deleteInstance(m_buildToolTipLayout);
		//
		//	}

		m_showBuildToolTipLayout = TRUE;
		//	m_buildToolTipLayout = TheWindowManager->winCreateLayout( "ControlBarPopupDescription.wnd" );
		//	m_buildToolTipLayout->setUpdate(ControlBarPopupDescriptionUpdateFunc);

		populateBuildTooltipLayout(commandButton);
	}
	else
	{
		// we're a generic window
		if(!BitIsSet(cmdButton->winGetStyle(), GWS_USER_WINDOW) && !BitIsSet(cmdButton->winGetStyle(), GWS_STATIC_TEXT))
			return;
		populateBuildTooltipLayout(nullptr, cmdButton);
	}
	m_buildToolTipLayout->hide(FALSE);

	if (useAnimation && TheGlobalData->m_animateWindows)
	{
		theAnimateWindowManager = NEW AnimateWindowManager;
		theAnimateWindowManager->reset();
		theAnimateWindowManager->registerGameWindow( m_buildToolTipLayout->getFirstWindow(), WIN_ANIMATION_SLIDE_RIGHT_FAST, TRUE, 200 );
	}


}

static Bool shouldUseUpgradeEconomyForTooltipObject(const ThingTemplate* tt)
{
	if (!tt)
		return FALSE;

	return tt->isKindOf(KINDOF_IGNORED_IN_GUI);
}

static void resetBuildTooltipLayoutToDefaults(WindowLayout* layout)
{
	if (!layout)
		return;

	GameWindow* root = layout->getFirstWindow();
	if (!root)
		return;

	GameWindow* costWin = TheWindowManager->winGetWindowFromId(
		root,
		TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextCost")
	);

	GameWindow* descWin = TheWindowManager->winGetWindowFromId(
		root,
		TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextDescription")
	);

	static Int s_tooltipDefaultDisplayWidth = 0;
	static Int s_tooltipDefaultDisplayHeight = 0;

	Int currentWidth = TheGlobalData ? TheGlobalData->m_xResolution : 0;
	Int currentHeight = TheGlobalData ? TheGlobalData->m_yResolution : 0;

	if (!s_tooltipSizesInitialized ||
		s_tooltipDefaultDisplayWidth != currentWidth ||
		s_tooltipDefaultDisplayHeight != currentHeight)
	{
		root->winGetSize(&s_tooltipRootDefaultSize.x, &s_tooltipRootDefaultSize.y);
		root->winGetPosition(&s_tooltipRootDefaultPos.x, &s_tooltipRootDefaultPos.y);

		if (costWin)
			costWin->winGetSize(&s_tooltipCostDefaultSize.x, &s_tooltipCostDefaultSize.y);

		if (descWin)
		{
			descWin->winGetSize(&s_tooltipDescDefaultSize.x, &s_tooltipDescDefaultSize.y);
			descWin->winGetPosition(&s_tooltipDescDefaultPos.x, &s_tooltipDescDefaultPos.y);
		}

		s_tooltipSizesInitialized = TRUE;
		s_tooltipDefaultDisplayWidth = currentWidth;
		s_tooltipDefaultDisplayHeight = currentHeight;
	}

	root->winSetSize(s_tooltipRootDefaultSize.x, s_tooltipRootDefaultSize.y);
	root->winSetPosition(s_tooltipRootDefaultPos.x, s_tooltipRootDefaultPos.y);

	if (costWin)
		costWin->winSetSize(s_tooltipCostDefaultSize.x, s_tooltipCostDefaultSize.y);

	if (descWin)
	{
		descWin->winSetSize(s_tooltipDescDefaultSize.x, s_tooltipDescDefaultSize.y);
		descWin->winSetPosition(s_tooltipDescDefaultPos.x, s_tooltipDescDefaultPos.y);
	}
}

static const StealthDetectorUpdateModuleData* getTooltipStealthDetectorData(
	const ThingTemplate* tt,
	const ThingTemplate** sourceThing)
{
	if (sourceThing)
		*sourceThing = nullptr;

	if (!tt)
		return nullptr;

	const StealthDetectorUpdateModuleData* directData =
		tt->friend_getStealthDetectorUpdateModuleData();

	if (directData)
	{
		if (sourceThing)
			*sourceThing = tt;
		return directData;
	}

	const ModuleInfo& behaviorModules = tt->getBehaviorModuleInfo();

	for (Int i = 0; i < behaviorModules.getCount(); ++i)
	{
		const ModuleData* moduleData = behaviorModules.getNthData(i);
		if (!moduleData)
			continue;

		AsciiString moduleName = behaviorModules.getNthName(i);
		if (moduleName.compareNoCase("SpawnBehavior") != 0)
			continue;

		const SpawnBehaviorModuleData* spawnData =
			static_cast<const SpawnBehaviorModuleData*>(moduleData);

		if (!spawnData)
			continue;

		for (std::vector<AsciiString>::const_iterator it = spawnData->m_spawnTemplateNameData.begin();
			it != spawnData->m_spawnTemplateNameData.end();
			++it)
		{
			const ThingTemplate* spawnedThing =
				TheThingFactory->findTemplate(*it);

			if (!spawnedThing)
				continue;

			const StealthDetectorUpdateModuleData* spawnedDetectorData =
				spawnedThing->friend_getStealthDetectorUpdateModuleData();

			if (spawnedDetectorData)
			{
				if (sourceThing)
					*sourceThing = spawnedThing;
				return spawnedDetectorData;
			}
		}
	}

	return nullptr;
}

static Bool getSupplyDropZoneIncomeForTooltip(
	const ThingTemplate* infoTemplate,
	Object* obj,
	Player* player,
	UnicodeString& outText)
{
	outText = UnicodeString::TheEmptyString;

	if (!infoTemplate || !infoTemplate->isKindOf(KINDOF_FS_SUPPLY_DROPZONE))
		return FALSE;

	const OCLUpdateModuleData* oclData = nullptr;

	if (obj)
	{
		static NameKeyType oclUpdateKey = TheNameKeyGenerator->nameToKey("OCLUpdate");
		OCLUpdate* oclUpdate = (OCLUpdate*)obj->findUpdateModule(oclUpdateKey);

		if (oclUpdate)
			oclData = oclUpdate->getOCLUpdateModuleDataForTooltip();
	}

	if (!oclData)
	{
		const ModuleInfo& modules = infoTemplate->getBehaviorModuleInfo();

		for (Int i = 0; i < modules.getCount(); ++i)
		{
			const ModuleData* moduleData = modules.getNthData(i);
			if (!moduleData)
				continue;

			AsciiString moduleName = modules.getNthName(i);
			if (moduleName.compareNoCase("OCLUpdate") != 0)
				continue;

			oclData = static_cast<const OCLUpdateModuleData*>(moduleData);
			break;
		}
	}

	if (!oclData || !oclData->m_ocl)
		return FALSE;

	AsciiString payloadName;
	Int payloadCount = 0;

	if (!oclData->m_ocl->getFirstDeliverPayloadForTooltip(payloadName, payloadCount))
		return FALSE;

	if (payloadName.isEmpty() || payloadCount <= 0)
		return FALSE;

	const ThingTemplate* crateTemplate = TheThingFactory->findTemplate(payloadName);
	if (!crateTemplate)
		return FALSE;

	const MoneyCrateCollideModuleData* moneyData = nullptr;

	const ModuleInfo& crateModules = crateTemplate->getBehaviorModuleInfo();
	for (Int i = 0; i < crateModules.getCount(); ++i)
	{
		const ModuleData* moduleData = crateModules.getNthData(i);
		if (!moduleData)
			continue;

		AsciiString moduleName = crateModules.getNthName(i);
		if (moduleName.compareNoCase("MoneyCrateCollide") != 0)
			continue;

		moneyData = static_cast<const MoneyCrateCollideModuleData*>(moduleData);
		break;
	}

	if (!moneyData)
		return FALSE;

	Int baseAmount = moneyData->m_moneyProvided * payloadCount;
	Int boostAmount = 0;
	UnicodeString boostUpgradeDisplay = UnicodeString::TheEmptyString;

	for (std::list<upgradePair>::const_iterator it = moneyData->m_upgradeBoost.begin(); it != moneyData->m_upgradeBoost.end(); ++it)
	{
		const UpgradeTemplate* boostUpgrade = TheUpgradeCenter->findUpgrade(it->type.c_str());
		if (!boostUpgrade)
			continue;

		if (boostUpgrade->getDisplayNameLabel().isNotEmpty())
			boostUpgradeDisplay = TheGameText->fetch(boostUpgrade->getDisplayNameLabel().str());
		else
			boostUpgradeDisplay.format(L"%S", it->type.c_str());

		if (player && player->hasUpgradeComplete(boostUpgrade))
			boostAmount = it->amount * payloadCount;

		break;
	}

	Int totalAmount = baseAmount + boostAmount;

	if (g_resourceMultiplierPercent != 100)
	{
		baseAmount = (baseAmount * g_resourceMultiplierPercent) / 100;
		boostAmount = (boostAmount * g_resourceMultiplierPercent) / 100;
		totalAmount = (totalAmount * g_resourceMultiplierPercent) / 100;
	}

	Int seconds = REAL_TO_INT_FLOOR((Real)oclData->m_minDelay / LOGICFRAMES_PER_SECOND + 0.5f);
	Real incomePerSecond = (seconds > 0) ? ((Real)totalAmount / (Real)seconds) : 0.0f;

	UnicodeString multiplierText = UnicodeString::TheEmptyString;
	if (g_resourceMultiplierPercent != 100)
		multiplierText.format(L" (x%.2f)", (Real)g_resourceMultiplierPercent / 100.0f);

	if (boostAmount > 0 && !boostUpgradeDisplay.isEmpty())
	{
		outText.format(L"Supply Drop Income: $%d every %d sec (%.1f/sec)%ls (Base: $%d, +$%d with %ls)",
			totalAmount,
			seconds,
			incomePerSecond,
			multiplierText.str(),
			baseAmount,
			boostAmount,
			boostUpgradeDisplay.str());
	}
	else if (!boostUpgradeDisplay.isEmpty())
	{
		Int potentialBoost = moneyData->m_upgradeBoost.empty() ? 0 : moneyData->m_upgradeBoost.front().amount * payloadCount;

		if (g_resourceMultiplierPercent != 100)
			potentialBoost = (potentialBoost * g_resourceMultiplierPercent) / 100;

		outText.format(L"Supply Drop Income: $%d every %d sec (%.1f/sec)%ls (Base: $%d, +$%d with %ls)",
			totalAmount,
			seconds,
			incomePerSecond,
			multiplierText.str(),
			baseAmount,
			potentialBoost,
			boostUpgradeDisplay.str());
	}
	else
	{
		outText.format(L"Supply Drop Income: $%d every %d sec (%.1f/sec)%ls",
			totalAmount,
			seconds,
			incomePerSecond,
			multiplierText.str());
	}

	return TRUE;
}

static Bool getInternetCenterContainedHackIncomeForTooltip(
	Object* internetCenter,
	Int& outHackerCount,
	Int& outTotalAmount,
	Int& outDelayFrames)
{
	outHackerCount = 0;
	outTotalAmount = 0;
	outDelayFrames = 0;

	if (!internetCenter || !internetCenter->isKindOf(KINDOF_FS_INTERNET_CENTER))
		return FALSE;

	static NameKeyType internetHackContainKey = TheNameKeyGenerator->nameToKey("InternetHackContain");
	InternetHackContain* contain = (InternetHackContain*)internetCenter->findUpdateModule(internetHackContainKey);

	if (!contain)
		return FALSE;

	const ContainedItemsList* items = contain->getContainedItemsListForTooltip();
	if (!items)
		return FALSE;

	for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it)
	{
		Object* passenger = *it;
		if (!passenger || !passenger->isKindOf(KINDOF_MONEY_HACKER))
			continue;

		static NameKeyType hackInternetKey = TheNameKeyGenerator->nameToKey("HackInternetAIUpdate");
		HackInternetAIUpdate* hackInternet = (HackInternetAIUpdate*)passenger->findUpdateModule(hackInternetKey);

		if (!hackInternet)
			continue;

		Int amount = hackInternet->getRegularCashAmount();

		if (passenger->getVeterancyLevel() == LEVEL_VETERAN)
			amount = hackInternet->getVeteranCashAmount();
		else if (passenger->getVeterancyLevel() == LEVEL_ELITE)
			amount = hackInternet->getEliteCashAmount();
		else if (passenger->getVeterancyLevel() == LEVEL_HEROIC)
			amount = hackInternet->getHeroicCashAmount();

		if (g_resourceMultiplierPercent != 100)
			amount = (amount * g_resourceMultiplierPercent) / 100;

		outTotalAmount += amount;
		++outHackerCount;

		if (outDelayFrames == 0)
			outDelayFrames = hackInternet->getCashUpdateDelay();
	}

	return outHackerCount > 0 && outTotalAmount > 0 && outDelayFrames > 0;
}

static const StealthDetectorUpdateModuleData* getSelectedUnitCameoStealthDetectorData(
	const ThingTemplate* tt,
	Object* selectedObject,
	const ThingTemplate** sourceThing)
{

	//DEBUG_LOG(("ENTER getSelectedUnitCameoStealthDetectorData tt=%s selectedObject=%p",
	//	tt ? tt->getName().str() : "<null>",
	//	selectedObject));

	if (sourceThing)
		*sourceThing = nullptr;

	if (!tt)
		return nullptr;

	const StealthDetectorUpdateModuleData* directData =
		getTooltipStealthDetectorData(tt, sourceThing);

	if (directData)
		return directData;

	if (!selectedObject)
		return nullptr;

	const ModuleInfo& behaviorModules = tt->getBehaviorModuleInfo();

	for (Int i = 0; i < behaviorModules.getCount(); ++i)
	{
		const ModuleData* moduleData = behaviorModules.getNthData(i);
		if (!moduleData)
			continue;

		AsciiString moduleName = behaviorModules.getNthName(i);
		if (moduleName.compareNoCase("ObjectCreationUpgrade") != 0)
			continue;

		const ObjectCreationUpgradeModuleData* ocuData =
			static_cast<const ObjectCreationUpgradeModuleData*>(moduleData);

		if (!ocuData || !ocuData->m_ocl)
			continue;

		//DEBUG_LOG(("SUC found ObjectCreationUpgrade on %s moduleTagName=%s activationCount=%d",
		//	tt->getName().str(),
		//	moduleName.str(),
		//	(Int)ocuData->m_upgradeMuxData.m_activationUpgradeNames.size()));

		const ThingTemplate* producedThing =
			ocuData->m_ocl->getFirstCreatedThingTemplate();

		//DEBUG_LOG(("SUC producedThing=%s",
		//	producedThing ? producedThing->getName().str() : "<null>"));

		//DEBUG_LOG(("SUC tooltipTrigger=%s",
		//	ocuData->m_tooltipTriggerUpgradeName.isEmpty() ? "<empty>" : ocuData->m_tooltipTriggerUpgradeName.str()));

		if (ocuData->m_tooltipTriggerUpgradeName.isEmpty())
			continue;

		const UpgradeTemplate* triggerUpgrade =
			TheUpgradeCenter->findUpgrade(ocuData->m_tooltipTriggerUpgradeName);

		//DEBUG_LOG(("SUC trigger=%s triggerUpgrade=%p",
		//	ocuData->m_tooltipTriggerUpgradeName.str(),
		//	triggerUpgrade));

		if (!triggerUpgrade)
			continue;

		//DEBUG_LOG(("SUC selectedObject=%s hasUpgrade=%d",
		//	selectedObject->getTemplate()->getName().str(),
		//	selectedObject->hasUpgrade(triggerUpgrade) ? 1 : 0));

		if (!selectedObject->hasUpgrade(triggerUpgrade))
			continue;


		if (!producedThing)
			continue;

		const StealthDetectorUpdateModuleData* producedDetectorData =
			getTooltipStealthDetectorData(producedThing, sourceThing);

		if (producedDetectorData)
		{
			if (sourceThing && *sourceThing == nullptr)
				*sourceThing = producedThing;
			return producedDetectorData;
		}
	}

	return nullptr;
}

static const ThingTemplate* getTooltipProducedThingTemplate(
	const CommandButton* commandButton,
	Object* selectedObject)
{
	if (!commandButton)
		return nullptr;

	if (commandButton->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE)
		return commandButton->getThingTemplate();

	const UpgradeTemplate* upgradeTemplate = commandButton->getUpgradeTemplate();
	if (upgradeTemplate && upgradeTemplate->getUpgradeName().compareNoCase("Upgrade_GLAWorkerFakeCommandSet") == 0)
		return commandButton->getThingTemplate();

	if (!selectedObject)
		return nullptr;

	if (!upgradeTemplate)
		return nullptr;

	const ThingTemplate* selectedTemplate = selectedObject->getTemplate();
	if (!selectedTemplate)
		return nullptr;

	const ModuleInfo& behaviorModules = selectedTemplate->getBehaviorModuleInfo();

	for (Int i = 0; i < behaviorModules.getCount(); ++i)
	{
		const ModuleData* moduleData = behaviorModules.getNthData(i);
		if (!moduleData)
			continue;

		AsciiString moduleName = behaviorModules.getNthName(i);
		if (moduleName.compareNoCase("ObjectCreationUpgrade") != 0)
			continue;

		const ObjectCreationUpgradeModuleData* ocuData =
			static_cast<const ObjectCreationUpgradeModuleData*>(moduleData);

		if (!ocuData || !ocuData->m_ocl)
			continue;

		Bool matchesUpgrade =
			ocuData->m_upgradeMuxData.isTriggeredBy(upgradeTemplate->getUpgradeName().str());

		if (!matchesUpgrade)
			continue;

		const ThingTemplate* producedThing = ocuData->m_ocl->getFirstCreatedThingTemplate();

		if (producedThing && producedThing->isKindOf(KINDOF_TAUNT))
			continue;

		if (producedThing)
			return producedThing;
	}

	return nullptr;
}

static UnicodeString getSidePrefixedThingName(const ThingTemplate* thingTemplate)
{
	if (!thingTemplate)
		return UnicodeString::TheEmptyString;

	UnicodeString result;

	if (!thingTemplate->getDisplayName().isEmpty())
		result = thingTemplate->getDisplayName();
	else
		result.translate(thingTemplate->getName());

	if (thingTemplate->getDefaultOwningSide().isNotEmpty())
	{
		UnicodeString sidePrefix;
		AsciiString rawSide = thingTemplate->getDefaultOwningSide();

		if (rawSide.compareNoCase("America") == 0)
			sidePrefix = L"USA";
		else if (rawSide.compareNoCase("AmericaSuperWeaponGeneral") == 0)
			sidePrefix = L"SupW";
		else if (rawSide.compareNoCase("AmericaLaserGeneral") == 0)
			sidePrefix = L"Laser";
		else if (rawSide.compareNoCase("AmericaAirForceGeneral") == 0)
			sidePrefix = L"AirF";
		else if (rawSide.compareNoCase("GLA") == 0)
			sidePrefix = L"GLA";
		else if (rawSide.compareNoCase("GLAToxinGeneral") == 0)
			sidePrefix = L"Toxin";
		else if (rawSide.compareNoCase("GLADemolitionGeneral") == 0)
			sidePrefix = L"Demo";
		else if (rawSide.compareNoCase("GLAStealthGeneral") == 0)
			sidePrefix = L"Slth";
		else if (rawSide.compareNoCase("China") == 0)
			sidePrefix = L"China";
		else if (rawSide.compareNoCase("ChinaTankGeneral") == 0)
			sidePrefix = L"Tank";
		else if (rawSide.compareNoCase("ChinaInfantryGeneral") == 0)
			sidePrefix = L"Infa";
		else if (rawSide.compareNoCase("ChinaNukeGeneral") == 0)
			sidePrefix = L"Nuke";
		else if (rawSide.compareNoCase("Boss") == 0)
			sidePrefix = L"Boss";

		if (!sidePrefix.isEmpty())
		{
			sidePrefix.concat(L" - ");
			sidePrefix.concat(result);
			result = sidePrefix;
		}
	}

	return result;
}


static UnicodeString getMaxHealthTriggerUpgradeDisplay(const MaxHealthUpgradeModuleData* maxHealthUpgradeData)
{
	if (!maxHealthUpgradeData)
		return L"an upgrade";

	AsciiString triggerUpgradeName = maxHealthUpgradeData->m_tooltipTriggerUpgradeName;

	if (triggerUpgradeName.isEmpty() && !maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.empty())
	{
		triggerUpgradeName = maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames[0];
	}

	if (triggerUpgradeName.isEmpty())
		return L"an upgrade";

	const UpgradeTemplate* triggerUpgradeTemplate = TheUpgradeCenter->findUpgrade(triggerUpgradeName);

	if (triggerUpgradeTemplate && triggerUpgradeTemplate->getDisplayNameLabel().isNotEmpty())
	{
		return TheGameText->fetch(triggerUpgradeTemplate->getDisplayNameLabel().str());
	}

	UnicodeString result;
	result.format(L"%S", triggerUpgradeName.str());
	return result;
}

static UnicodeString getPowerPlantTriggerUpgradeDisplay(const PowerPlantUpgradeModuleData* powerPlantUpgradeData)
{
	if (!powerPlantUpgradeData)
		return L"overcharge";

	AsciiString triggerUpgradeName = powerPlantUpgradeData->m_tooltipTriggerUpgradeName;

	if (triggerUpgradeName.isEmpty() && !powerPlantUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.empty())
	{
		triggerUpgradeName = powerPlantUpgradeData->m_upgradeMuxData.m_activationUpgradeNames[0];
	}

	if (triggerUpgradeName.isEmpty())
		return L"overcharge";

	const UpgradeTemplate* triggerUpgradeTemplate = TheUpgradeCenter->findUpgrade(triggerUpgradeName);

	if (triggerUpgradeTemplate && triggerUpgradeTemplate->getDisplayNameLabel().isNotEmpty())
	{
		return TheGameText->fetch(triggerUpgradeTemplate->getDisplayNameLabel().str());
	}

	UnicodeString result;
	result.format(L"%S", triggerUpgradeName.str());
	return result;
}

static const LocomotorSetUpgradeModuleData* getLocomotorSetUpgradeModuleData(const ThingTemplate* thingTemplate)
{
	if (!thingTemplate)
		return nullptr;

	const ModuleInfo& behaviorModules = thingTemplate->getBehaviorModuleInfo();

	for (Int i = 0; i < behaviorModules.getCount(); ++i)
	{
		const ModuleData* moduleData = behaviorModules.getNthData(i);
		if (!moduleData)
			continue;

		AsciiString moduleName = behaviorModules.getNthName(i);
		if (moduleName.compareNoCase("LocomotorSetUpgrade") != 0)
			continue;

		return static_cast<const LocomotorSetUpgradeModuleData*>(moduleData);
	}

	return nullptr;
}

static const LocomotorTemplate* getFirstLocomotorTemplateForSet(const ThingTemplate* thingTemplate, LocomotorSetType setType)
{
	if (!thingTemplate)
		return nullptr;

	AIUpdateModuleData* aiData = const_cast<ThingTemplate*>(thingTemplate)->friend_getAIModuleInfo();
	if (!aiData)
		return nullptr;

	const LocomotorTemplateVector* locoVector = aiData->findLocomotorTemplateVector(setType);
	if (!locoVector || locoVector->empty())
		return nullptr;

	return (*locoVector)[0];
}

static Bool getDisplaySpeedForLocomotorSet(const ThingTemplate* thingTemplate, LocomotorSetType setType, Int& outDisplaySpeed)
{
	outDisplaySpeed = 0;

	const LocomotorTemplate* locoTemplate = getFirstLocomotorTemplateForSet(thingTemplate, setType);
	if (!locoTemplate)
		return FALSE;

	Locomotor* tempLocomotor = TheLocomotorStore->newLocomotor(locoTemplate);
	if (!tempLocomotor)
		return FALSE;

	Real speed = tempLocomotor->getMaxSpeedForCondition(BODY_PRISTINE);
	Real displaySpeed = speed * LOGICFRAMES_PER_SECOND;

	deleteInstance(tempLocomotor);

	if (displaySpeed <= 0.0f)
		return FALSE;

	outDisplaySpeed = REAL_TO_INT_FLOOR(displaySpeed + 0.5f);
	return TRUE;
}

static UnicodeString getLocomotorTriggerUpgradeDisplay(const LocomotorSetUpgradeModuleData* locomotorSetUpgradeData)
{
	if (!locomotorSetUpgradeData)
		return L"an upgrade";

	AsciiString triggerUpgradeName = locomotorSetUpgradeData->m_tooltipTriggerUpgradeName;

	if (triggerUpgradeName.isEmpty() && !locomotorSetUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.empty())
	{
		triggerUpgradeName = locomotorSetUpgradeData->m_upgradeMuxData.m_activationUpgradeNames[0];
	}

	if (triggerUpgradeName.isEmpty())
		return L"an upgrade";

	const UpgradeTemplate* triggerUpgradeTemplate = TheUpgradeCenter->findUpgrade(triggerUpgradeName);

	if (triggerUpgradeTemplate && triggerUpgradeTemplate->getDisplayNameLabel().isNotEmpty())
	{
		return TheGameText->fetch(triggerUpgradeTemplate->getDisplayNameLabel().str());
	}

	UnicodeString result;
	result.format(L"%S", triggerUpgradeName.str());
	return result;
}

static Bool getDisplaySpeedForLocomotorSetAndCondition(
	const ThingTemplate* thingTemplate,
	LocomotorSetType setType,
	BodyDamageType damageType,
	Int& outDisplaySpeed)
{
	outDisplaySpeed = 0;

	const LocomotorTemplate* locoTemplate = getFirstLocomotorTemplateForSet(thingTemplate, setType);
	if (!locoTemplate)
		return FALSE;

	Locomotor* tempLocomotor = TheLocomotorStore->newLocomotor(locoTemplate);
	if (!tempLocomotor)
		return FALSE;

	Real speed = tempLocomotor->getMaxSpeedForCondition(damageType);
	Real displaySpeed = speed * LOGICFRAMES_PER_SECOND;

	deleteInstance(tempLocomotor);

	if (displaySpeed <= 0.0f)
		return FALSE;

	outDisplaySpeed = REAL_TO_INT_FLOOR(displaySpeed + 0.5f);
	return TRUE;
}

void ControlBar::repopulateBuildTooltipLayout()
{
	if(!prevWindow || !m_buildToolTipLayout)
		return;
	if(!BitIsSet(prevWindow->winGetStyle(), GWS_PUSH_BUTTON))
		return;
	const CommandButton *commandButton = (const CommandButton *)GadgetButtonGetData(prevWindow);
	populateBuildTooltipLayout(commandButton);
}

void ControlBar::populateBuildTooltipLayout( const CommandButton *commandButton, GameWindow *tooltipWin)
{
	if(!m_buildToolTipLayout)
		return;

	resetBuildTooltipLayoutToDefaults(m_buildToolTipLayout);

	Player* player = ThePlayerList->getLocalPlayer();
	UnicodeString name, cost, buildtime, powertext, limittext, reloadtext, descrip, healthText, shroudText, locomotorText, stealthDetectText, autoDepositText, hackInternetText, supplyDropText;
	buildtime = UnicodeString::TheEmptyString;
	powertext = UnicodeString::TheEmptyString;
	limittext = UnicodeString::TheEmptyString;
	reloadtext = UnicodeString::TheEmptyString;
	healthText = UnicodeString::TheEmptyString;
	stealthDetectText = UnicodeString::TheEmptyString;
	autoDepositText = UnicodeString::TheEmptyString;
	hackInternetText = UnicodeString::TheEmptyString;
	supplyDropText = UnicodeString::TheEmptyString;

	UnicodeString requiresFormat = UnicodeString::TheEmptyString, requiresList;
	Bool firstRequirement = true;
	const ProductionPrerequisite* prereq;
	Bool fireScienceButton = false;
	UnsignedInt costToBuild = 0;
	Real buildTimeValue = 0.0f;
	Int costDiffSize = 0;

	Bool useUpgradeEconomyForIgnoredGuiObject = FALSE;

	if(commandButton)
	{
		////const ThingTemplate *thingTemplate = commandButton->getThingTemplate();
		//thingTemplate = commandButton->getThingTemplate();
		//const UpgradeTemplate *upgradeTemplate = commandButton->getUpgradeTemplate();
		//const SpecialPowerTemplate* specialPowerTemplate = commandButton->getSpecialPowerTemplate();




		Drawable* draw = TheInGameUI->getFirstSelectedDrawable();
		Object* selectedObject = draw ? draw->getObject() : nullptr;

		thingTemplate = getTooltipProducedThingTemplate(commandButton, selectedObject);

		const ThingTemplate* buildTemplate = thingTemplate;
		const ThingTemplate* infoTemplate = thingTemplate;



		const UpgradeTemplate* upgradeTemplate = commandButton->getUpgradeTemplate();
		const SpecialPowerTemplate* specialPowerTemplate = commandButton->getSpecialPowerTemplate();

		if (commandButton->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE &&
			upgradeTemplate &&
			thingTemplate &&
			shouldUseUpgradeEconomyForTooltipObject(thingTemplate))
		{
			useUpgradeEconomyForIgnoredGuiObject = TRUE;
		}

		if (thingTemplate && !useUpgradeEconomyForIgnoredGuiObject)
		{
			const std::vector<AsciiString>& buildVariations = thingTemplate->getBuildVariations();

			if (!buildVariations.empty())
			{
				const ThingTemplate* firstVariationTemplate = TheThingFactory->findTemplate(buildVariations[0]);
				if (firstVariationTemplate)
					infoTemplate = firstVariationTemplate;
			}
		}


		ScienceType	st = SCIENCE_INVALID;
		if( commandButton->getCommandType() != GUI_COMMAND_PLAYER_UPGRADE &&
				commandButton->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE )
		{
			if( commandButton->getScienceVec().size() > 1 )
			{
				for(size_t j = 0; j < commandButton->getScienceVec().size(); ++j)
				{
					st = commandButton->getScienceVec()[ j ];

					if( commandButton->getCommandType() != GUI_COMMAND_PURCHASE_SCIENCE )
					{
						if( !player->hasScience( st ) && j > 0 )
						{
							//If we're not looking at a command button that purchases a science, then
							//it means we are looking at a command button that can USE the science. This
							//means we want to get the description for the previous science -- the one
							//we can use, not purchase!
							st = commandButton->getScienceVec()[ j - 1 ];
						}

						//Now that we got the science for the button that executes the science, we need
						//to generate a simpler help text!
						fireScienceButton = TRUE;

						break;
					}
					else if( !player->hasScience( st ) )
					{
						//Purchase science case. The first science we run into that we don't have, that's the
						//one we'll want to show!
						break;
					}
				}
			}
			else if(commandButton->getScienceVec().size() == 1 )
			{
				st = commandButton->getScienceVec()[ 0 ];
				if( commandButton->getCommandType() != GUI_COMMAND_PURCHASE_SCIENCE )
				{
					//Now that we got the science for the button that executes the science, we need
					//to generate a simpler help text!
					fireScienceButton = TRUE;
				}
			}
		}

		if( commandButton->getDescriptionLabel().isNotEmpty() )
		{
			descrip = TheGameText->fetch(commandButton->getDescriptionLabel());

			Drawable *draw = TheInGameUI->getFirstSelectedDrawable();
			Object *selectedObject = draw ? draw->getObject() : nullptr;
			if( selectedObject )
			{
				//Special case: Append status of overcharge on China power plant.
				if( commandButton->getCommandType() == GUI_COMMAND_TOGGLE_OVERCHARGE )
				{
					{
						OverchargeBehaviorInterface *obi;
						for( BehaviorModule **bmi = selectedObject->getBehaviorModules(); *bmi; ++bmi )
						{
							obi = (*bmi)->getOverchargeBehaviorInterface();
							if( obi )
							{
								descrip.concat( L"\n" );
								if( obi->isOverchargeActive() )
									descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipNukeReactorOverChargeIsOn" ) );
								else
									descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipNukeReactorOverChargeIsOff" ) );
							}
						}
					}
				}

				//Special case: When building units & buildings, the CanMakeType determines reasons for not being able to buy stuff.
				//else if( thingTemplate )
				else if (thingTemplate &&
					commandButton->getCommandType() != GUI_COMMAND_PURCHASE_SCIENCE &&
					!useUpgradeEconomyForIgnoredGuiObject)
				{
					CanMakeType makeType = TheBuildAssistant->canMakeUnit( selectedObject, commandButton->getThingTemplate() );
					switch( makeType )
					{
						case CANMAKE_NO_MONEY:
							descrip.concat( L"\n\n" );
							descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipNotEnoughMoneyToBuild" ) );
							break;
						case CANMAKE_QUEUE_FULL:
							descrip.concat( L"\n\n" );
							descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipCannotPurchaseBecauseQueueFull" ) );
							break;
						case CANMAKE_PARKING_PLACES_FULL:
							descrip.concat( L"\n\n" );
							descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipCannotBuildUnitBecauseParkingFull" ) );
							break;
						case CANMAKE_MAXED_OUT_FOR_PLAYER:
							descrip.concat( L"\n\n" );
              if ( thingTemplate->isKindOf( KINDOF_STRUCTURE ) )
              {
                descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipCannotBuildBuildingBecauseMaximumNumber" ) );
              }
              else
              {
  							descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipCannotBuildUnitBecauseMaximumNumber" ) );
              }
							break;
						//case CANMAKE_NO_PREREQ:
						//	descrip.concat( L"\n\n" );
						//	descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipCannotBuildDueToPrerequisites" ) );
						//	break;
					}
				}
				else if (thingTemplate && upgradeTemplate && commandButton->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE)
				{
					Bool alreadyUpgraded = false;
					Bool conflictingUpgrade = false;

					if (selectedObject)
					{
						alreadyUpgraded = selectedObject->hasUpgrade(upgradeTemplate);
						conflictingUpgrade = !selectedObject->affectedByUpgrade(upgradeTemplate);
					}

					if (alreadyUpgraded)
					{
						descrip = TheGameText->fetch("TOOLTIP:AlreadyUpgradedDefault");
					}
					else if (conflictingUpgrade)
					{
						if (commandButton->getConflictingLabel().isNotEmpty())
							descrip = TheGameText->fetch(commandButton->getConflictingLabel());
						else
							descrip = TheGameText->fetch("TOOLTIP:HasConflictingUpgradeDefault");
					}
					else if (!player->hasUpgradeInProduction(upgradeTemplate))
					{
						ProductionUpdateInterface* pui = selectedObject->getProductionUpdateInterface();
						if (pui && pui->getProductionCount() == MAX_BUILD_QUEUE_BUTTONS)
						{
							descrip.concat(L"\n\n");
							descrip.concat(TheGameText->fetch("TOOLTIP:TooltipCannotPurchaseBecauseQueueFull"));
						}
						else if (!TheUpgradeCenter->canAffordUpgrade(ThePlayerList->getLocalPlayer(), upgradeTemplate, FALSE))
						{
							descrip.concat(L"\n\n");
							descrip.concat(TheGameText->fetch("TOOLTIP:TooltipNotEnoughMoneyToBuild"));
						}
					}




				}



				


				//Special case: When building upgrades
				else if( upgradeTemplate && !player->hasUpgradeInProduction( upgradeTemplate ) )
				{
					if( commandButton->getCommandType() == GUI_COMMAND_PLAYER_UPGRADE ||
						  commandButton->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE )
					{
						ProductionUpdateInterface *pui = selectedObject->getProductionUpdateInterface();
						if( pui && pui->getProductionCount() == MAX_BUILD_QUEUE_BUTTONS )
						{
							descrip.concat( L"\n\n" );
							descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipCannotPurchaseBecauseQueueFull" ) );
						}
						else if( !TheUpgradeCenter->canAffordUpgrade( ThePlayerList->getLocalPlayer(), upgradeTemplate, FALSE ) )
						{
							descrip.concat( L"\n\n" );
							descrip.concat( TheGameText->fetch( "TOOLTIP:TooltipNotEnoughMoneyToBuild" ) );
						}
					}
				}


				if (thingTemplate)
				{
					const ThingTemplate* stealthTemplate =
						useUpgradeEconomyForIgnoredGuiObject ? thingTemplate : infoTemplate;

					const ThingTemplate* stealthSourceThing = nullptr;
					const StealthDetectorUpdateModuleData* stealthDetectorData =
						getTooltipStealthDetectorData(stealthTemplate, &stealthSourceThing);

					Bool allowSpawnProvidedStealthTooltip =
						(stealthSourceThing == nullptr) ||
						(stealthSourceThing == stealthTemplate) ||
						stealthTemplate->isKindOf(KINDOF_SPAWNS_ARE_THE_WEAPONS) ||
						useUpgradeEconomyForIgnoredGuiObject;

					if (stealthDetectorData && allowSpawnProvidedStealthTooltip)
					{
						Real stealthDetectRange = stealthDetectorData->m_detectionRange;

						if (stealthDetectRange <= 0.0f)
						{
							stealthDetectRange = stealthTemplate->getVisionRangeForTooltip();
						}

						if (stealthDetectRange > 0.0f)
						{
							if (stealthSourceThing && stealthSourceThing != stealthTemplate)
							{
								UnicodeString sourceName;

								if (!stealthSourceThing->getDisplayName().isEmpty())
									sourceName = stealthSourceThing->getDisplayName();
								else
									sourceName.translate(stealthSourceThing->getName());

								stealthDetectText.format(
									L"Stealth Detection Range: %.0f (via %ls)",
									stealthDetectRange,
									sourceName.str());
							}
							else
							{
								stealthDetectText.format(L"Stealth Detection Range: %.0f", stealthDetectRange);
							}
						}
					}
				}

			}

		}

		GUICommandType commandType = commandButton->getCommandType();

		// Reborn: This is a special case because the button is only disabled when the rally point is already at default, so we want to show that in the tooltip.
		if (commandType == GUI_COMMAND_RESET_RALLY_POINT)
		{
			Drawable* draw = TheInGameUI->getFirstSelectedDrawable();
			Object* selectedObject = draw ? draw->getObject() : nullptr;

			if (selectedObject)
			{
				ExitInterface* exit = selectedObject->getObjectExitInterface();
				if (exit && exit->getRallyPoint() == nullptr)
				{
					descrip = L"No custom rally point set";
				}
			}
		}

		if (
			specialPowerTemplate &&
			(
				commandType == GUI_COMMAND_SPECIAL_POWER ||
				commandType == GUI_COMMAND_SPECIAL_POWER_FROM_SHORTCUT ||
				commandType == GUI_COMMAND_SPECIAL_POWER_CONSTRUCT ||
				commandType == GUI_COMMAND_SPECIAL_POWER_CONSTRUCT_FROM_SHORTCUT
				)
			)
		{
			Real reloadTimeValue = specialPowerTemplate->getReloadTime() / (Real)LOGICFRAMES_PER_SECOND;

			Real remainingTimeValue = 0.0f;

			Drawable* draw = TheInGameUI->getFirstSelectedDrawable();
			Object* selectedObject = draw ? draw->getObject() : nullptr;

			//SpecialPowerType spType = specialPowerTemplate->getSpecialPowerType();
			//Object* cooldownObject = selectedObject;
			//SpecialPowerModuleInterface* spm = nullptr;

			//if (cooldownObject)
			//{
			//	spm = cooldownObject->findSpecialPowerModuleInterface(spType);
			//}

			//if (!spm)
			//{
			//	cooldownObject = player->findBestSpecialPowerSourceObject(specialPowerTemplate);

			//	if (cooldownObject)
			//	{
			//		spm = cooldownObject->findSpecialPowerModuleInterface(spType);
			//	}
			//}
			Object* cooldownObject = selectedObject;
			SpecialPowerModuleInterface* spm = nullptr;

			if (cooldownObject)
			{
				spm = cooldownObject->getSpecialPowerModule(specialPowerTemplate);
			}

			if (!spm)
			{
				cooldownObject = player->findBestSpecialPowerSourceObject(specialPowerTemplate);

				if (cooldownObject)
				{
					spm = cooldownObject->getSpecialPowerModule(specialPowerTemplate);
				}
			}

			if (spm)
			{
				UnsignedInt readyFrame = spm->getReadyFrame();
				UnsignedInt currentFrame = TheGameLogic->getFrame();

				if (readyFrame > currentFrame)
				{
					UnsignedInt remainingFrames = readyFrame - currentFrame;
					remainingTimeValue = remainingFrames / (Real)LOGICFRAMES_PER_SECOND;
				}
			}

			Int reloadTimeDisplay = REAL_TO_INT_FLOOR(reloadTimeValue + 0.5f);
			Int remainingTimeDisplay = REAL_TO_INT_FLOOR(remainingTimeValue + 0.5f);

			if (reloadTimeValue > 0.0f)
			{
				if (remainingTimeValue > 0.0f)
				{
					reloadtext.format(L"Reload Time: %d sec (%d sec remaining)",
						reloadTimeDisplay,
						remainingTimeDisplay);
				}
				else
				{
					reloadtext.format(L"Reload Time: %d sec (Ready)",
						reloadTimeDisplay);
				}
			}
		}

		name = TheGameText->fetch(commandButton->getTextLabel().str());

		if (commandButton->getCommandType() == GUI_COMMAND_SELL)
		{
			Drawable* draw = TheInGameUI->getFirstSelectedDrawable();
			Object* selectedObject = draw ? draw->getObject() : nullptr;

			if (selectedObject)
			{
				const ThingTemplate* selectedTemplate = selectedObject->getTemplate();
				if (selectedTemplate)
				{
					UnsignedInt costToBuild = selectedTemplate->calcCostToBuild(player);
					UnsignedInt refundValue = selectedTemplate->getRefundValue();

					if (refundValue == 0)
						refundValue = costToBuild / 2;

					Int percent = 0;
					if (costToBuild > 0)
						percent = (refundValue * 100) / costToBuild;

					cost.format(L"Refund Value: %d (%d%% of original cost)", refundValue, percent);
				}
			}
		}
		else if (thingTemplate &&
			commandButton->getCommandType() != GUI_COMMAND_PURCHASE_SCIENCE &&
			!useUpgradeEconomyForIgnoredGuiObject)
		{

			//We are either looking at building a unit or a structure that may or may not have any
			//prerequisites.

			//Format the cost only when we have to pay for it.
			Int baseCost = buildTemplate->friend_getBuildCost();
			costToBuild = buildTemplate->calcCostToBuild(player);

			if (costToBuild > 0)
			{
				cost.format(TheGameText->fetch("TOOLTIP:Cost"), costToBuild);

				if (baseCost > 0 && costToBuild != baseCost)
				{
					Int percentDiff = ((Int)costToBuild - baseCost) * 100 / baseCost;
					UnicodeString costModifierText;

					if (percentDiff < 0)
					{
						costModifierText.format(L" (%d%% cheaper then original)", -percentDiff);
					}
					else if (percentDiff > 0)
					{
						costModifierText.format(L" (%d%% more expensive then original)", percentDiff);
					}

					cost.concat(costModifierText);
				}
			}



			buildTimeValue = buildTemplate->calcTimeToBuild(player) / (Real)LOGICFRAMES_PER_SECOND;
			if (buildTimeValue > 0.0f)
			{
				if (buildTimeValue == (Int)buildTimeValue)
				{
					buildtime.format(L"Build Time: %d sec", (Int)buildTimeValue);
				}
				else
				{
					buildtime.format(L"Build Time: %.1f sec", buildTimeValue);
				}
				if (player->getEnergy() && player->getEnergy()->getEnergySupplyRatio() < 1.0f)
				{
					buildtime.concat(L" (Low Power)");
				}
				else
				{
					buildtime.concat(L" (Powered)");
				}
			}

			Real energyValue = infoTemplate->getEnergyProduction();
			Real energyBonusValue = infoTemplate->getEnergyBonus();

			if (energyValue != 0.0f)
			{
				if (energyValue < 0.0f)
				{
					if (energyValue == (Int)energyValue)
					{
						powertext.format(L"Power Consumption: %d", (Int)(-energyValue));
					}
					else
					{
						powertext.format(L"Power Consumption: %.1f", -energyValue);
					}
				}
				else
				{
					if (energyValue == (Int)energyValue)
					{
						powertext.format(L"Power Production: %d", (Int)energyValue);
					}
					else
					{
						powertext.format(L"Power Production: %.1f", energyValue);
					}

					if (energyBonusValue > 0.0f)
					{
						const PowerPlantUpgradeModuleData* powerPlantUpgradeData = nullptr;

						const ModuleInfo& behaviorModules = infoTemplate->getBehaviorModuleInfo();
						for (Int i = 0; i < behaviorModules.getCount(); ++i)
						{
							const ModuleData* moduleData = behaviorModules.getNthData(i);
							if (!moduleData)
								continue;

							AsciiString moduleName = behaviorModules.getNthName(i);
							if (moduleName.compareNoCase("PowerPlantUpgrade") != 0)
								continue;

							powerPlantUpgradeData = static_cast<const PowerPlantUpgradeModuleData*>(moduleData);
							break;
						}

						UnicodeString triggerUpgradeDisplay = getPowerPlantTriggerUpgradeDisplay(powerPlantUpgradeData);

						UnicodeString bonusText;
						if (energyBonusValue == (Int)energyBonusValue)
						{
							bonusText.format(L" (+%d Bonus with %ls)", (Int)energyBonusValue, triggerUpgradeDisplay.str());
						}
						else
						{
							bonusText.format(L" (+%.1f Bonus with %ls)", energyBonusValue, triggerUpgradeDisplay.str());
						}
						powertext.concat(bonusText);
					}
				}
			}

			const ActiveBodyModuleData* bodyData = infoTemplate->friend_getActiveBodyModuleData();
			const MaxHealthUpgradeModuleData* maxHealthUpgradeData = infoTemplate->friend_getMaxHealthUpgradeModuleData();

			//DEBUG_LOG(("HealthTooltip template=%s maxHealthUpgradeData=%08X activationCount=%d",
			//	thingTemplate ? thingTemplate->getName().str() : "null",
			//	maxHealthUpgradeData,
			//	maxHealthUpgradeData ? (Int)maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.size() : -1));

			if (maxHealthUpgradeData)
			{
				for (size_t debugUpgradeIndex = 0; debugUpgradeIndex < maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.size(); ++debugUpgradeIndex)
				{
					AsciiString debugUpgradeName = maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames[debugUpgradeIndex];

					//DEBUG_LOG(("HealthTooltip template=%s activationUpgrade[%d]=%s",
					//	thingTemplate ? thingTemplate->getName().str() : "null",
					//	(Int)debugUpgradeIndex,
					//	debugUpgradeName.str()));

					const UpgradeTemplate* debugUpgradeTemplate = TheUpgradeCenter->findUpgrade(debugUpgradeName);

					if (debugUpgradeTemplate)
					{
						//DEBUG_LOG(("HealthTooltip template=%s activationUpgrade[%d] displayLabel=%s",
						//	thingTemplate ? thingTemplate->getName().str() : "null",
						//	(Int)debugUpgradeIndex,
						//	debugUpgradeTemplate->getDisplayNameLabel().str()));
					}
					else
					{
						//DEBUG_LOG(("HealthTooltip template=%s activationUpgrade[%d] upgradeTemplate=null",
						//	thingTemplate ? thingTemplate->getName().str() : "null",
						//	(Int)debugUpgradeIndex));
					}
				}
			}

			if (infoTemplate && infoTemplate->isKindOf(KINDOF_MOB_NEXUS))
			{
				Real spawnMinHealth = 0.0f;
				Real spawnMaxHealth = 0.0f;

				if (SpawnBehavior::getPotentialSpawnHealthForTooltip(infoTemplate, spawnMinHealth, spawnMaxHealth))
				{
					if (spawnMinHealth > 0.0f && spawnMinHealth != spawnMaxHealth)
					{
						healthText.format(L"Health: %.0f - %.0f", spawnMinHealth, spawnMaxHealth);
					}
					else
					{
						healthText.format(L"Health: %.0f", spawnMaxHealth);
					}
				}
			}
			else if (bodyData)
			{
				if (maxHealthUpgradeData && maxHealthUpgradeData->m_addMaxHealth > 0.0f)
				{
					UnicodeString triggerUpgradeDisplay = getMaxHealthTriggerUpgradeDisplay(maxHealthUpgradeData);

					healthText.format(L"Health: %.0f (+%.0f with %ls)",
						bodyData->m_maxHealth,
						maxHealthUpgradeData->m_addMaxHealth,
						triggerUpgradeDisplay.str());
				}
				else
				{
					healthText.format(L"Health: %.0f", bodyData->m_maxHealth);
				}
			}

			if (infoTemplate->getShroudClearingRange() > 0.0f)
			{
				Real shroudRange = infoTemplate->getShroudClearingRange();
				shroudText.format(L"Shroud Clearing Range: %.0f", shroudRange);

				Drawable* draw = TheInGameUI->getFirstSelectedDrawable();
				Object* selectedObject = draw ? draw->getObject() : nullptr;

				if (selectedObject)
				{
					Player* selectedPlayer = selectedObject->getControllingPlayer();

					if (selectedPlayer && selectedPlayer->getBattlePlansActiveSpecific(PLANSTATUS_SEARCHANDDESTROY) > 0)
					{
						Real scalar = selectedPlayer->getBattlePlanSightRangeScalar();

						if (scalar > 1.0f)
						{
							Int percentBonus = (Int)((scalar - 1.0f) * 100.0f + 0.5f);
							Real bonusAmount = shroudRange * (scalar - 1.0f);

							UnicodeString shroudBonusText;
							shroudBonusText.format(L" (+%.0f, +%d%% with Search and Destroy)", bonusAmount, percentBonus);
							shroudText.concat(shroudBonusText);
						}
					}
				}
			}

			if (!infoTemplate->isKindOf(KINDOF_STRUCTURE))
			{
				Int normalDisplaySpeed = 0;
				Int upgradedDisplaySpeed = 0;

				Bool hasNormalSpeed = getDisplaySpeedForLocomotorSet(infoTemplate, LOCOMOTORSET_NORMAL, normalDisplaySpeed);
				Bool hasUpgradedSpeed = getDisplaySpeedForLocomotorSet(infoTemplate, LOCOMOTORSET_NORMAL_UPGRADED, upgradedDisplaySpeed);

				const LocomotorSetUpgradeModuleData* locomotorSetUpgradeData =
					getLocomotorSetUpgradeModuleData(infoTemplate);

				if (hasNormalSpeed)
				{
					if (hasUpgradedSpeed && locomotorSetUpgradeData && upgradedDisplaySpeed != normalDisplaySpeed)
					{
						UnicodeString triggerUpgradeDisplay =
							getLocomotorTriggerUpgradeDisplay(locomotorSetUpgradeData);

						locomotorText.format(
							L"Movement Speed: %d (%d with %ls)",
							normalDisplaySpeed,
							upgradedDisplaySpeed,
							triggerUpgradeDisplay.str());
					}
					else
					{
						locomotorText.format(L"Movement Speed: %d", normalDisplaySpeed);
					}
				}
			}



			const AutoDepositUpdateModuleData* autoDepositData = nullptr;

			const ModuleInfo& autoDepositBehaviorModules = infoTemplate->getBehaviorModuleInfo();
			for (Int i = 0; i < autoDepositBehaviorModules.getCount(); ++i)
			{
				const ModuleData* moduleData = autoDepositBehaviorModules.getNthData(i);
				if (!moduleData)
					continue;

				AsciiString moduleName = autoDepositBehaviorModules.getNthName(i);
				if (moduleName.compareNoCase("AutoDepositUpdate") != 0)
					continue;

				autoDepositData = static_cast<const AutoDepositUpdateModuleData*>(moduleData);
				break;
			}

			if (autoDepositData)
			{
				Int baseAmount = autoDepositData->m_depositAmount;
				Int displayAmount = baseAmount;
				Int potentialBoost = 0;
				UnicodeString boostUpgradeDisplay = UnicodeString::TheEmptyString;

				for (std::list<upgradePair>::const_iterator it = autoDepositData->m_upgradeBoost.begin(); it != autoDepositData->m_upgradeBoost.end(); ++it)
				{
					const UpgradeTemplate* boostUpgrade = TheUpgradeCenter->findUpgrade(it->type.c_str());

					if (!boostUpgrade)
						continue;

					potentialBoost = it->amount;

					if (boostUpgrade->getDisplayNameLabel().isNotEmpty())
						boostUpgradeDisplay = TheGameText->fetch(boostUpgrade->getDisplayNameLabel().str());
					else
						boostUpgradeDisplay.format(L"%S", it->type.c_str());

					if (player && player->hasUpgradeComplete(boostUpgrade))
					{
						displayAmount += it->amount;
					}

					break;
				}

				Int displayBaseAmount = baseAmount;
				Int displayBoostAmount = potentialBoost;

				if (g_resourceMultiplierPercent != 100)
				{
					displayAmount = (displayAmount * g_resourceMultiplierPercent) / 100;
					displayBaseAmount = (displayBaseAmount * g_resourceMultiplierPercent) / 100;
					displayBoostAmount = (displayBoostAmount * g_resourceMultiplierPercent) / 100;
				}

				Int seconds = REAL_TO_INT_FLOOR((Real)autoDepositData->m_depositFrame / LOGICFRAMES_PER_SECOND + 0.5f);
				Real incomePerSecond = (seconds > 0) ? ((Real)displayAmount / (Real)seconds) : 0.0f;

				UnicodeString multiplierText = UnicodeString::TheEmptyString;
				if (g_resourceMultiplierPercent != 100)
				{
					multiplierText.format(L" (x%.2f)", (Real)g_resourceMultiplierPercent / 100.0f);
				}

				if (displayBoostAmount > 0 && !boostUpgradeDisplay.isEmpty())
				{
					autoDepositText.format(L"Auto Income: $%d every %d sec (%.1f/sec)%ls (Base: $%d, +$%d with %ls)",
						displayAmount,
						seconds,
						incomePerSecond,
						multiplierText.str(),
						displayBaseAmount,
						displayBoostAmount,
						boostUpgradeDisplay.str());
				}
				else
				{
					autoDepositText.format(L"Auto Income: $%d every %d sec (%.1f/sec)%ls",
						displayAmount,
						seconds,
						incomePerSecond,
						multiplierText.str());
				}
			}


			const HackInternetAIUpdateModuleData* hackInternetData = nullptr;

			const ModuleInfo& hackInternetBehaviorModules = infoTemplate->getBehaviorModuleInfo();
			for (Int i = 0; i < hackInternetBehaviorModules.getCount(); ++i)
			{
				const ModuleData* moduleData = hackInternetBehaviorModules.getNthData(i);
				if (!moduleData)
					continue;

				AsciiString moduleName = hackInternetBehaviorModules.getNthName(i);
				if (moduleName.compareNoCase("HackInternetAIUpdate") != 0)
					continue;

				hackInternetData = static_cast<const HackInternetAIUpdateModuleData*>(moduleData);
				break;
			}

			if (hackInternetData)
			{
				Int regularAmount = hackInternetData->m_regularCashAmount;
				Int veteranAmount = hackInternetData->m_veteranCashAmount;
				Int eliteAmount = hackInternetData->m_eliteCashAmount;
				Int heroicAmount = hackInternetData->m_heroicCashAmount;

				if (g_resourceMultiplierPercent != 100)
				{
					regularAmount = (regularAmount * g_resourceMultiplierPercent) / 100;
					veteranAmount = (veteranAmount * g_resourceMultiplierPercent) / 100;
					eliteAmount = (eliteAmount * g_resourceMultiplierPercent) / 100;
					heroicAmount = (heroicAmount * g_resourceMultiplierPercent) / 100;
				}

				Int seconds = REAL_TO_INT_FLOOR((Real)hackInternetData->m_cashUpdateDelay / LOGICFRAMES_PER_SECOND + 0.5f);
				Real incomePerSecond = (seconds > 0) ? ((Real)regularAmount / (Real)seconds) : 0.0f;

				UnicodeString multiplierText = UnicodeString::TheEmptyString;
				if (g_resourceMultiplierPercent != 100)
				{
					multiplierText.format(L" (x%.2f)", (Real)g_resourceMultiplierPercent / 100.0f);
				}

				hackInternetText.format(L"Hack Income: $%d every %d sec (%.1f/sec)%ls\nRanks: Vet $%d, Elite $%d, Heroic $%d",
					regularAmount,
					seconds,
					incomePerSecond,
					multiplierText.str(),
					veteranAmount,
					eliteAmount,
					heroicAmount);
			}


			getSupplyDropZoneIncomeForTooltip(infoTemplate, nullptr, player, supplyDropText);

UnsignedInt buildLimit = thingTemplate->getMaxSimultaneousOfType();
if (buildLimit > 0)
{
	Int currentCount = 0;
	const ThingTemplate* tmpl = thingTemplate;
	player->countObjectsByThingTemplate(1, &tmpl, false, &currentCount);

	Int queuedCount = 0;
	Int effectiveCount = currentCount;

	Drawable *draw = TheInGameUI->getFirstSelectedDrawable();
	Object *selectedObject = draw ? draw->getObject() : nullptr;
	if (selectedObject)
	{
		ProductionUpdateInterface *pui = selectedObject->getProductionUpdateInterface();
		if (pui)
		{
			queuedCount = pui->countUnitTypeInQueue(thingTemplate);
		}

		effectiveCount = currentCount + queuedCount;

		CanMakeType makeType = TheBuildAssistant->canMakeUnit(selectedObject, thingTemplate);
		if (makeType == CANMAKE_MAXED_OUT_FOR_PLAYER && effectiveCount < (Int)buildLimit)
		{
			effectiveCount = buildLimit;
		}
	}
	else
	{
		effectiveCount = currentCount;
	}

	limittext.format(L"Build Limit: %d/%d", effectiveCount, buildLimit);
}


if (commandButton->getCommandType() != GUI_COMMAND_OBJECT_UPGRADE)
{
	// ask each prerequisite to give us a list of the non satisfied prerequisites
	for (Int i = 0; i < thingTemplate->getPrereqCount(); i++)
	{
		prereq = thingTemplate->getNthPrereq(i);
		//requiresList = prereq->getRequiresList(player);
		requiresList = prereq->getRequiresList(player);

		if (requiresList != UnicodeString::TheEmptyString && prereq->getNumUnitPrereqs() > 0)
		{
			UnicodeString taggedRequiresList = UnicodeString::TheEmptyString;

			for (Int prereqIndex = 0; prereqIndex < prereq->getNumUnitPrereqs(); ++prereqIndex)
			{
				const ThingTemplate* prereqThing = prereq->getUnitPrereq(prereqIndex);
				if (!prereqThing)
					continue;

				UnicodeString taggedName = getSidePrefixedThingName(prereqThing);

				if (!taggedRequiresList.isEmpty())
					taggedRequiresList.concat(L", ");

				taggedRequiresList.concat(taggedName);
			}

			if (!taggedRequiresList.isEmpty())
				requiresList = taggedRequiresList;
		}

		if (requiresList != UnicodeString::TheEmptyString)
		{
			// make sure to put in 'returns' to space things correctly
			if (firstRequirement)
				firstRequirement = false;
			else
				requiresFormat.concat(L", ");
		}
		requiresFormat.concat(requiresList);
	}

}

			if( !requiresFormat.isEmpty() )
			{
				UnicodeString requireFormat = TheGameText->fetch("CONTROLBAR:Requirements");
				requiresFormat.format(requireFormat.str(), requiresFormat.str());
				if(!descrip.isEmpty())
					descrip.concat(L"\n");
				descrip.concat(requiresFormat);

			}
		}
		else if( upgradeTemplate )
		{
			//We are looking at an upgrade purchase icon. Maybe we already purchased it?

			Bool hasUpgradeAlready = player->hasUpgradeComplete( upgradeTemplate );
			Bool hasConflictingUpgrade = FALSE;
			Bool missingScience = FALSE;
			Bool playerUpgradeButton = commandButton->getCommandType() == GUI_COMMAND_PLAYER_UPGRADE;
			Bool objectUpgradeButton = commandButton->getCommandType() == GUI_COMMAND_OBJECT_UPGRADE;

			if( !hasUpgradeAlready )
			{
				//Check if the first selected object has the specified upgrade.
				Drawable *draw = TheInGameUI->getFirstSelectedDrawable();
				if( draw )
				{
					Object *object = draw->getObject();
					if( object )
					{
						hasUpgradeAlready = object->hasUpgrade( upgradeTemplate );
						if( objectUpgradeButton )
						{
							hasConflictingUpgrade = !object->affectedByUpgrade( upgradeTemplate );
						}
					}
				}
			}
			if( hasConflictingUpgrade && !hasUpgradeAlready )
			{
				if( commandButton->getConflictingLabel().isNotEmpty() )
				{
					descrip = TheGameText->fetch( commandButton->getConflictingLabel() );
				}
				else
				{
					descrip = TheGameText->fetch( "TOOLTIP:HasConflictingUpgradeDefault" );
				}
			}
			else if( hasUpgradeAlready && ( playerUpgradeButton || objectUpgradeButton ) )
			{
				//See if we can fetch the "already upgraded" text for this upgrade. If not.... use the default "fill me in".
				if( commandButton->getPurchasedLabel().isNotEmpty() )
				{
					descrip = TheGameText->fetch( commandButton->getPurchasedLabel() );
				}
				else
				{
					descrip = TheGameText->fetch( "TOOLTIP:AlreadyUpgradedDefault" );
				}
			}
			else if( !hasUpgradeAlready )
			{

				//Do we have a prerequisite science?
				for( size_t i = 0; i < commandButton->getScienceVec().size(); i++ )
				{
					ScienceType st = commandButton->getScienceVec()[ i ];
					if( !player->hasScience( st ) )
					{
						missingScience = TRUE;
						break;
					}
				}

				//Determine the cost of the upgrade.
				costToBuild = upgradeTemplate->calcCostToBuild( player );
				if( costToBuild > 0 )
				{
					cost.format( TheGameText->fetch("TOOLTIP:Cost"), costToBuild );
				}

				buildTimeValue = upgradeTemplate->calcTimeToBuild(player) / (Real)LOGICFRAMES_PER_SECOND;
				if (buildTimeValue > 0.0f)
				{
					if (buildTimeValue == (Int)buildTimeValue)
					{
						buildtime.format(L"Research Time: %d sec", (Int)buildTimeValue);
					}
					else
					{
						buildtime.format(L"Research Time: %.1f sec", buildTimeValue);
					}
				}

				if( missingScience )
				{
					if( !descrip.isEmpty() )
						descrip.concat(L"\n");
					requiresFormat.format( TheGameText->fetch( "CONTROLBAR:Requirements" ).str(), TheGameText->fetch( "CONTROLBAR:GeneralsPromotion" ).str() );
					descrip.concat( requiresFormat );
				}
			}
		}
		else if( st != SCIENCE_INVALID && !fireScienceButton )
		{
			TheScienceStore->getNameAndDescription(st, name, descrip);

			costToBuild = TheScienceStore->getSciencePurchaseCost( st );
			if( costToBuild > 0 )
			{
				cost.format( TheGameText->fetch("TOOLTIP:ScienceCost"), costToBuild );
			}

			// ask each prerequisite to give us a list of the non satisfied prerequisites
			if( thingTemplate )
			{
				for( Int i=0; i<thingTemplate->getPrereqCount(); i++ )
				{
					prereq = thingTemplate->getNthPrereq(i);
					requiresList = prereq->getRequiresList(player);

					if (requiresList != UnicodeString::TheEmptyString && prereq->getNumUnitPrereqs() > 0)
					{
						UnicodeString taggedRequiresList = UnicodeString::TheEmptyString;

						for (Int prereqIndex = 0; prereqIndex < prereq->getNumUnitPrereqs(); ++prereqIndex)
						{
							const ThingTemplate* prereqThing = prereq->getUnitPrereq(prereqIndex);
							if (!prereqThing)
								continue;

							UnicodeString taggedName = getSidePrefixedThingName(prereqThing);

							if (!taggedRequiresList.isEmpty())
								taggedRequiresList.concat(L", ");

							taggedRequiresList.concat(taggedName);
						}

						if (!taggedRequiresList.isEmpty())
							requiresList = taggedRequiresList;
					}

					if( requiresList != UnicodeString::TheEmptyString )
					{
						// make sure to put in 'returns' to space things correctly
						if (firstRequirement)
							firstRequirement = false;
						else
							requiresFormat.concat(L", ");
					}
					requiresFormat.concat(requiresList);
				}
				if( !requiresFormat.isEmpty() )
				{
					UnicodeString requireFormat = TheGameText->fetch("CONTROLBAR:Requirements");
					requiresFormat.format(requireFormat.str(), requiresFormat.str());
					if(!descrip.isEmpty())
						descrip.concat(L"\n");
					descrip.concat(requiresFormat);
				}
			}

		}
	}
	else if(tooltipWin)
	{

		if( tooltipWin == TheWindowManager->winGetWindowFromId(m_buildToolTipLayout->getFirstWindow(), TheNameKeyGenerator->nameToKey("ControlBar.wnd:MoneyDisplay")))
		{
			name = TheGameText->fetch("CONTROLBAR:Money");
			descrip = TheGameText->fetch("CONTROLBAR:MoneyDescription");
		}
		else if(tooltipWin == TheWindowManager->winGetWindowFromId(m_buildToolTipLayout->getFirstWindow(), TheNameKeyGenerator->nameToKey("ControlBar.wnd:PowerWindow")) )
		{
			name = TheGameText->fetch("CONTROLBAR:Power");
			descrip = TheGameText->fetch("CONTROLBAR:PowerDescription");

			Player* playerToDisplay = getCurrentlyViewedPlayer();

			if( playerToDisplay && playerToDisplay->getEnergy() )
			{
				Energy *energy = playerToDisplay->getEnergy();
				descrip.format(descrip, energy->getProduction(), energy->getConsumption());
			}
			else
			{
				descrip.format(descrip, 0, 0);
			}
		}
		else if(tooltipWin == TheWindowManager->winGetWindowFromId(m_buildToolTipLayout->getFirstWindow(), TheNameKeyGenerator->nameToKey("ControlBar.wnd:GeneralsExp")) )
		{
			name = TheGameText->fetch("CONTROLBAR:GeneralsExp");
			descrip = TheGameText->fetch("CONTROLBAR:GeneralsExpDescription");
		}
		else
		{
			DEBUG_CRASH(("ControlBar::populateBuildTooltipLayout We attempted to call the popup tooltip on a game window that has yet to be hand coded in as this fuction was/is designed for only buttons but has been hacked to work with GameWindows."));
			return;
		}

	}

	if (thingTemplate && thingTemplate->getDefaultOwningSide().isNotEmpty())
	{
		UnicodeString sidePrefix;
		AsciiString rawSide = thingTemplate->getDefaultOwningSide();

		if (rawSide.compareNoCase("America") == 0)
			sidePrefix = L"USA";
		else if (rawSide.compareNoCase("AmericaSuperWeaponGeneral") == 0)
			sidePrefix = L"SupW";
		else if (rawSide.compareNoCase("AmericaLaserGeneral") == 0)
			sidePrefix = L"Laser";
		else if (rawSide.compareNoCase("AmericaAirForceGeneral") == 0)
			sidePrefix = L"AirF";
		else if (rawSide.compareNoCase("GLA") == 0)
			sidePrefix = L"GLA";
		else if (rawSide.compareNoCase("GLAToxinGeneral") == 0)
			sidePrefix = L"Toxin";
		else if (rawSide.compareNoCase("GLADemolitionGeneral") == 0)
			sidePrefix = L"Demo";
		else if (rawSide.compareNoCase("GLAStealthGeneral") == 0)
			sidePrefix = L"Slth";
		else if (rawSide.compareNoCase("China") == 0)
			sidePrefix = L"China";
		else if (rawSide.compareNoCase("ChinaTankGeneral") == 0)
			sidePrefix = L"Tank";
		else if (rawSide.compareNoCase("ChinaInfantryGeneral") == 0)
			sidePrefix = L"Infa";
		else if (rawSide.compareNoCase("ChinaNukeGeneral") == 0)
			sidePrefix = L"Nuke";
		else if (rawSide.compareNoCase("Boss") == 0)
			sidePrefix = L"Boss";

		if (!sidePrefix.isEmpty())
		{
			sidePrefix.concat(L" - ");
			sidePrefix.concat(name);
			name = sidePrefix;
		}
	}

	GameWindow *win = TheWindowManager->winGetWindowFromId(m_buildToolTipLayout->getFirstWindow(), TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextName"));
	if(win)
	{
		GadgetStaticTextSetText(win, name);
	}

	win = TheWindowManager->winGetWindowFromId(m_buildToolTipLayout->getFirstWindow(), TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextCost"));
	if(win)
	{
		if (!cost.isEmpty() || !buildtime.isEmpty() || !reloadtext.isEmpty() || !powertext.isEmpty() || !limittext.isEmpty() || !autoDepositText.isEmpty() || !supplyDropText.isEmpty())
		{
			win->winHide(FALSE);

			UnicodeString costAndTime = UnicodeString::TheEmptyString;

			if (!cost.isEmpty())
			{
				costAndTime.concat(cost);
			}

			if (!buildtime.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(buildtime);
			}

			if (!reloadtext.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(reloadtext);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !healthText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(healthText);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !powertext.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(powertext);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !shroudText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(shroudText);
			}

			if (!stealthDetectText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(stealthDetectText);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !locomotorText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(locomotorText);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !autoDepositText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(autoDepositText);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !hackInternetText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(hackInternetText);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !supplyDropText.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(supplyDropText);
			}

			if (!useUpgradeEconomyForIgnoredGuiObject && !limittext.isEmpty())
			{
				if (!costAndTime.isEmpty())
					costAndTime.concat(L"\n");
				costAndTime.concat(limittext);
			}

			costAndTime.concat(L"\n");

			ICoord2D size, newSize;
			DisplayString* tempDString = TheDisplayStringManager->newDisplayString();
			win->winGetSize(&size.x, &size.y);
			tempDString->setFont(win->winGetFont());
			tempDString->setWordWrap(size.x - 10);
			tempDString->setText(costAndTime);
			tempDString->getSize(&newSize.x, &newSize.y);
			TheDisplayStringManager->freeDisplayString(tempDString);
			tempDString = nullptr;

			costDiffSize = newSize.y - size.y;
			if (costDiffSize < 0)
				costDiffSize = 0;

			win->winSetSize(size.x, size.y + costDiffSize);
			GadgetStaticTextSetText(win, costAndTime);
		}
		else
		{
			win->winHide(TRUE);
		}
	}



	win = TheWindowManager->winGetWindowFromId(m_buildToolTipLayout->getFirstWindow(), TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextDescription"));
	if(win)
	{

		static NameKeyType winNamekey	= TheNameKeyGenerator->nameToKey( "ControlBar.wnd:BackgroundMarker" );
		static ICoord2D lastOffset = { 0, 0 };

		ICoord2D size, newSize, pos;
		Int diffSize;

		ICoord2D descPos;
		win->winGetPosition(&descPos.x, &descPos.y);
		win->winSetPosition(descPos.x, descPos.y + costDiffSize);

		DisplayString *tempDString = TheDisplayStringManager->newDisplayString();
		win->winGetSize(&size.x, &size.y);
		tempDString->setFont(win->winGetFont());
		tempDString->setWordWrap(size.x - 10);
		tempDString->setText(descrip);
		tempDString->getSize(&newSize.x, &newSize.y);
		TheDisplayStringManager->freeDisplayString(tempDString);
		tempDString = nullptr;
		diffSize = newSize.y - size.y;
 		GameWindow *parent = m_buildToolTipLayout->getFirstWindow();
 		if(!parent)
 			return;

 		parent->winGetSize(&size.x, &size.y);
 	//	if(size.y + diffSize < 102) {
		//	diffSize = 102 - size.y;
		//}
		if (size.y + diffSize < 64) {
			diffSize = 64 - size.y;
		}

		parent->winSetSize(size.x, size.y + costDiffSize + diffSize);
 		parent->winGetPosition(&pos.x, &pos.y);
//		if(size.y + diffSize < 102)
//		{
//
//			parent->winSetPosition(pos.x, pos.y -  (102 - (newSize.y + size.y + diffSize) ));
//		}
//		else

//		heightChange = controlBarPos.y - m_defaultControlBarPosition.y;

		GameWindow *marker =  TheWindowManager->winGetWindowFromId(nullptr,winNamekey);
		static ICoord2D basePos;
		if(!marker)
		{
			return;
		}
		getBackgroundMarkerPos(&basePos.x, &basePos.y);
		ICoord2D curPos, offset;
		marker->winGetScreenPosition(&curPos.x,&curPos.y);

		offset.x = curPos.x - basePos.x;
		offset.y = curPos.y - basePos.y;

		parent->winSetPosition(pos.x, (pos.y - costDiffSize - diffSize) + (offset.y - lastOffset.y));

		lastOffset.x = offset.x;
		lastOffset.y = offset.y;

		win->winGetSize(&size.x, &size.y);
 		win->winSetSize(size.x, size.y + diffSize);

		GadgetStaticTextSetText(win, descrip);
	}
	m_buildToolTipLayout->hide(FALSE);
}


void ControlBar::showSelectedUnitTooltipLayout(GameWindow* window, Object* obj)
{
	if (!window || !obj || !m_buildToolTipLayout)
		return;

	if (TheInGameUI->areTooltipsDisabled() || TheScriptEngine->isGameEnding())
		return;

	// Reborn: We want to allow tooltips in replay
	//if (TheGameLogic->isInReplayGame())
	//	return;

	if (TheInGameUI->isQuitMenuVisible())
		return;

	if (TheDisconnectMenu && TheDisconnectMenu->isScreenVisible())
		return;

	resetBuildTooltipLayoutToDefaults(m_buildToolTipLayout);

	m_showBuildToolTipLayout = TRUE;

	Player* player = ThePlayerList->getLocalPlayer();
	const ThingTemplate* thing = obj->getTemplate();
	if (!player || !thing)
		return;

	UnicodeString name, cost, buildtime, powertext, limittext, healthText, shroudText, locomotorText, stealthDetectText, autoDepositText, hackInternetText, internetCenterHackText, supplyDropText, infoText, descrip;
	descrip = UnicodeString::TheEmptyString;
	stealthDetectText = UnicodeString::TheEmptyString;

	if (!thing->getDisplayName().isEmpty())
		name = thing->getDisplayName();
	else
		name.translate(thing->getName());

	if (thing->getDefaultOwningSide().isNotEmpty())
	{
		UnicodeString sidePrefix;
		AsciiString rawSide = thing->getDefaultOwningSide();

		if (rawSide.compareNoCase("America") == 0)
			sidePrefix = L"USA";
		else if (rawSide.compareNoCase("AmericaSuperWeaponGeneral") == 0)
			sidePrefix = L"SupW";
		else if (rawSide.compareNoCase("AmericaLaserGeneral") == 0)
			sidePrefix = L"Laser";
		else if (rawSide.compareNoCase("AmericaAirForceGeneral") == 0)
			sidePrefix = L"AirF";
		else if (rawSide.compareNoCase("GLA") == 0)
			sidePrefix = L"GLA";
		else if (rawSide.compareNoCase("GLAToxinGeneral") == 0)
			sidePrefix = L"Toxin";
		else if (rawSide.compareNoCase("GLADemolitionGeneral") == 0)
			sidePrefix = L"Demo";
		else if (rawSide.compareNoCase("GLAStealthGeneral") == 0)
			sidePrefix = L"Slth";
		else if (rawSide.compareNoCase("China") == 0)
			sidePrefix = L"China";
		else if (rawSide.compareNoCase("ChinaTankGeneral") == 0)
			sidePrefix = L"Tank";
		else if (rawSide.compareNoCase("ChinaInfantryGeneral") == 0)
			sidePrefix = L"Infa";
		else if (rawSide.compareNoCase("ChinaNukeGeneral") == 0)
			sidePrefix = L"Nuke";
		else if (rawSide.compareNoCase("Boss") == 0)
			sidePrefix = L"Boss";

		if (!sidePrefix.isEmpty())
		{
			sidePrefix.concat(L" - ");
			sidePrefix.concat(name);
			name = sidePrefix;
		}
	}


	Player* localPlayer = ThePlayerList->getLocalPlayer();
	Player* ownerPlayer = obj ? obj->getControllingPlayer() : nullptr;

	BodyModuleInterface* liveBody = obj ? obj->getBodyModule() : nullptr;

	Real currentHealthForTooltip = liveBody ? liveBody->getHealth() : 0.0f;
	Real maxHealthForTooltip = liveBody ? liveBody->getMaxHealth() : 0.0f;
	Bool useScaledEnemyHealthLine = FALSE;

	SpawnBehaviorInterface* spawnInterface = obj ? obj->getSpawnBehaviorInterface() : nullptr;

	if (spawnInterface && thing->isKindOf(KINDOF_MOB_NEXUS))
	{
		Real slaveCurrentHealth = 0.0f;
		Real slaveMaxHealth = 0.0f;

		if (spawnInterface->getSlavesHealthForTooltip(slaveCurrentHealth, slaveMaxHealth))
		{
			currentHealthForTooltip = slaveCurrentHealth;
			maxHealthForTooltip = slaveMaxHealth;
		}
	}

	if (localPlayer && ownerPlayer && localPlayer->getRelationship(ownerPlayer->getDefaultTeam()) == ENEMIES)
	{
		if (thing && thing->isKindOf(KINDOF_FS_FAKE) && thing->getRebornFakeBaseObject().isNotEmpty())
		{
			const ThingTemplate* fakeBaseThing = TheThingFactory->findTemplate(thing->getRebornFakeBaseObject());
			const ActiveBodyModuleData* fakeBaseBodyData = fakeBaseThing ? fakeBaseThing->friend_getActiveBodyModuleData() : nullptr;

			if (fakeBaseBodyData && liveBody && liveBody->getMaxHealth() > 0.0f)
			{
				Real fakeBaseMaxHealth = fakeBaseBodyData->m_maxHealth;
				Real selectedObjectMaxHealth = liveBody->getMaxHealth();

				if (fakeBaseMaxHealth > 0.0f && selectedObjectMaxHealth > 0.0f)
				{
					Real healthScale = fakeBaseMaxHealth / selectedObjectMaxHealth;

					currentHealthForTooltip = liveBody->getHealth() * healthScale;
					maxHealthForTooltip = liveBody->getMaxHealth() * healthScale;
					useScaledEnemyHealthLine = TRUE;
				}
			}
		}
	}

	const ActiveBodyModuleData* bodyData = thing->friend_getActiveBodyModuleData();
	const MaxHealthUpgradeModuleData* maxHealthUpgradeData = thing->friend_getMaxHealthUpgradeModuleData();
	
	//DEBUG_LOG(("SelectedTooltip thing=%s maxHealthUpgradeData=%08X activationCount=%d",
	//	thing ? thing->getName().str() : "null",
	//	maxHealthUpgradeData,
	//	maxHealthUpgradeData ? (Int)maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.size() : -1));

	if (maxHealthUpgradeData)
	{
		for (size_t debugUpgradeIndex = 0; debugUpgradeIndex < maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.size(); ++debugUpgradeIndex)
		{
			AsciiString debugUpgradeName = maxHealthUpgradeData->m_upgradeMuxData.m_activationUpgradeNames[debugUpgradeIndex];

			//DEBUG_LOG(("SelectedTooltip thing=%s activationUpgrade[%d]=%s",
			//	thing ? thing->getName().str() : "null",
			//	(Int)debugUpgradeIndex,
			//	debugUpgradeName.str()));

			const UpgradeTemplate* debugUpgradeTemplate = TheUpgradeCenter->findUpgrade(debugUpgradeName);

			if (debugUpgradeTemplate)
			{
				//DEBUG_LOG(("SelectedTooltip thing=%s activationUpgrade[%d] displayLabel=%s",
				//	thing ? thing->getName().str() : "null",
				//	(Int)debugUpgradeIndex,
				//	debugUpgradeTemplate->getDisplayNameLabel().str()));
			}
			else
			{
				//DEBUG_LOG(("SelectedTooltip thing=%s activationUpgrade[%d] upgradeTemplate=null",
				//	thing ? thing->getName().str() : "null",
				//	(Int)debugUpgradeIndex));
			}
		}
	}

	if (thing->isKindOf(KINDOF_SUPPLY_SOURCE))
	{
		static NameKeyType warehouseModuleKey = TheNameKeyGenerator->nameToKey("SupplyWarehouseDockUpdate");
		SupplyWarehouseDockUpdate* warehouseModule = (SupplyWarehouseDockUpdate*)obj->findUpdateModule(warehouseModuleKey);

		if (warehouseModule)
		{
			Int boxes = warehouseModule->getBoxesStored();
			Int value = boxes * TheGlobalData->m_baseValuePerSupplyBox;

			if (g_resourceMultiplierPercent != 100)
				value = (value * g_resourceMultiplierPercent) / 100;

			const SupplyWarehouseDockUpdateModuleData* warehouseData = warehouseModule->getSupplyWarehouseDockUpdateModuleDataForTooltip();

			Int initialBoxes = warehouseData ? warehouseData->m_startingBoxesData : boxes;
			Int initialValue = initialBoxes * TheGlobalData->m_baseValuePerSupplyBox;

			if (g_resourceMultiplierPercent != 100)
				initialValue = (initialValue * g_resourceMultiplierPercent) / 100;

			if (g_resourceMultiplierPercent != 100)
			{
				healthText.format(L"Resources: $%d / $%d (with x%.2f Resource Multiplier)",
					value,
					initialValue,
					(Real)g_resourceMultiplierPercent / 100.0f);
			}
			else
			{
				healthText.format(L"Resources: $%d / $%d", value, initialValue);
			}
		}
	}
	else if (bodyData && liveBody)
	{
		Real currentHealth = currentHealthForTooltip;
		Real baseMaxHealth = bodyData->m_maxHealth;
		Real displayMaxHealth = maxHealthForTooltip;

		if (maxHealthUpgradeData && maxHealthUpgradeData->m_addMaxHealth > 0.0f)
		{
			UnicodeString triggerUpgradeDisplay = getMaxHealthTriggerUpgradeDisplay(maxHealthUpgradeData);

			healthText.format(L"Health: %.0f / %.0f (Base: %.0f, +%.0f with %ls)",
				currentHealth,
				displayMaxHealth,
				baseMaxHealth,
				maxHealthUpgradeData->m_addMaxHealth,
				triggerUpgradeDisplay.str());
		}
		else
		{
			healthText.format(L"Health: %.0f / %.0f",
				currentHealth,
				displayMaxHealth);
		}
	}

	UnicodeString powerText = UnicodeString::TheEmptyString;

	Real baseEnergyValue = thing->getEnergyProduction();
	Real bonusEnergyValue = 0.0f;
	Real displayEnergyValue = baseEnergyValue;

	if (baseEnergyValue > 0.0f)
	{
		static NameKeyType powerPlantUpgradeKey = NAMEKEY("PowerPlantUpgrade");

		for (BehaviorModule** bmi = obj->getBehaviorModules(); *bmi; ++bmi)
		{
			UpgradeModuleInterface* upgradeMod = (*bmi)->getUpgrade();
			if (!upgradeMod)
				continue;

			if ((*bmi)->getModuleNameKey() == powerPlantUpgradeKey && upgradeMod->isAlreadyUpgraded())
			{
				bonusEnergyValue = thing->getEnergyBonus();
				displayEnergyValue += bonusEnergyValue;
				break;
			}
		}

		if (bonusEnergyValue == 0.0f)
		{
			for (BehaviorModule** bmi = obj->getBehaviorModules(); *bmi; ++bmi)
			{
				OverchargeBehaviorInterface* obi = (*bmi)->getOverchargeBehaviorInterface();
				if (!obi)
					continue;

				if (obi->isOverchargeActive())
				{
					bonusEnergyValue = thing->getEnergyBonus();
					displayEnergyValue += bonusEnergyValue;
				}
				break;
			}
		}
	}

	if (displayEnergyValue != 0.0f)
	{
		if (displayEnergyValue < 0.0f)
		{
			powerText.format(L"Power Consumption: %.0f", -displayEnergyValue);
		}
		else
		{
			if (bonusEnergyValue > 0.0f)
			{
				UnicodeString bonusSourceText = L"Bonus";

				const PowerPlantUpgradeModuleData* powerPlantUpgradeData = nullptr;
				Bool overchargeActive = FALSE;

				const ModuleInfo& behaviorModules = thing->getBehaviorModuleInfo();
				for (Int i = 0; i < behaviorModules.getCount(); ++i)
				{
					const ModuleData* moduleData = behaviorModules.getNthData(i);
					if (!moduleData)
						continue;

					AsciiString moduleName = behaviorModules.getNthName(i);
					if (moduleName.compareNoCase("PowerPlantUpgrade") != 0)
						continue;

					powerPlantUpgradeData =
						static_cast<const PowerPlantUpgradeModuleData*>(moduleData);
					break;
				}

				for (BehaviorModule** bmi = obj->getBehaviorModules(); *bmi; ++bmi)
				{
					OverchargeBehaviorInterface* obi = (*bmi)->getOverchargeBehaviorInterface();
					if (obi && obi->isOverchargeActive())
					{
						overchargeActive = TRUE;
						break;
					}
				}

				if (powerPlantUpgradeData)
				{
					UnicodeString triggerUpgradeDisplay =
						getPowerPlantTriggerUpgradeDisplay(powerPlantUpgradeData);

					bonusSourceText.format(L"Bonus with %ls", triggerUpgradeDisplay.str());
				}
				else if (overchargeActive)
				{
					bonusSourceText = L"Bonus from Overcharge";
				}

				powerText.format(L"Power Production: %.0f (Base: %.0f, +%.0f %ls)",
					displayEnergyValue,
					baseEnergyValue,
					bonusEnergyValue,
					bonusSourceText.str());
			}
			else
			{
				powerText.format(L"Power Production: %.0f", displayEnergyValue);
			}
		}
	}



if (thing->getShroudClearingRange() > 0.0f)
{
	Real shroudRange = thing->getShroudClearingRange();
	shroudText.format(L"Shroud Clearing Range: %.0f", shroudRange);

	Player* selectedPlayer = obj->getControllingPlayer();

	if (selectedPlayer && selectedPlayer->getBattlePlansActiveSpecific(PLANSTATUS_SEARCHANDDESTROY) > 0)
	{
		Real scalar = selectedPlayer->getBattlePlanSightRangeScalar();

		if (scalar > 1.0f)
		{
			Int percentBonus = (Int)((scalar - 1.0f) * 100.0f + 0.5f);
			Real bonusAmount = shroudRange * (scalar - 1.0f);

			UnicodeString shroudBonusText;
			shroudBonusText.format(L" (+%.0f, +%d%% with Search and Destroy)", bonusAmount, percentBonus);
			shroudText.concat(shroudBonusText);
		}
	}
}

const ThingTemplate* stealthSourceThing = nullptr;
const StealthDetectorUpdateModuleData* stealthDetectorData =
getSelectedUnitCameoStealthDetectorData(thing, obj, &stealthSourceThing);

if (stealthDetectorData)
{
	Real stealthDetectRange = stealthDetectorData->m_detectionRange;

	if (stealthDetectRange <= 0.0f)
	{
		const ThingTemplate* rangeThing = stealthSourceThing ? stealthSourceThing : thing;
		if (rangeThing)
			stealthDetectRange = rangeThing->getVisionRangeForTooltip();
	}

	if (stealthDetectRange > 0.0f)
	{
		if (stealthSourceThing && stealthSourceThing != thing)
		{
			UnicodeString sourceName;

			if (!stealthSourceThing->getDisplayName().isEmpty())
				sourceName = stealthSourceThing->getDisplayName();
			else
				sourceName.translate(stealthSourceThing->getName());

			stealthDetectText.format(
				L"Stealth Detection Range: %.0f (via %ls)",
				stealthDetectRange,
				sourceName.str());
		}
		else
		{
			stealthDetectText.format(L"Stealth Detection Range: %.0f", stealthDetectRange);
		}
	}
}




if (obj && !thing->isKindOf(KINDOF_STRUCTURE))
{
	Int baseMaxDisplaySpeed = 0;
	Int upgradedMaxDisplaySpeed = 0;
	Int activeMaxDisplaySpeed = 0;
	Int damagedMaxDisplaySpeed = 0;

	Bool hasBaseMax =
		getDisplaySpeedForLocomotorSetAndCondition(thing, LOCOMOTORSET_NORMAL, BODY_PRISTINE, baseMaxDisplaySpeed);

	Bool hasUpgradedMax =
		getDisplaySpeedForLocomotorSetAndCondition(thing, LOCOMOTORSET_NORMAL_UPGRADED, BODY_PRISTINE, upgradedMaxDisplaySpeed);

	const LocomotorSetUpgradeModuleData* locomotorSetUpgradeData =
		getLocomotorSetUpgradeModuleData(thing);

	Bool hasLocomotorUpgrade = FALSE;
	UnicodeString triggerUpgradeDisplay = L"an upgrade";

	if (locomotorSetUpgradeData)
	{
		triggerUpgradeDisplay = getLocomotorTriggerUpgradeDisplay(locomotorSetUpgradeData);

		AsciiString triggerUpgradeName = locomotorSetUpgradeData->m_tooltipTriggerUpgradeName;
		if (triggerUpgradeName.isEmpty() && !locomotorSetUpgradeData->m_upgradeMuxData.m_activationUpgradeNames.empty())
		{
			triggerUpgradeName = locomotorSetUpgradeData->m_upgradeMuxData.m_activationUpgradeNames[0];
		}

		if (!triggerUpgradeName.isEmpty())
		{
			const UpgradeTemplate* triggerUpgrade = TheUpgradeCenter->findUpgrade(triggerUpgradeName);
			Player* controllingPlayer = obj->getControllingPlayer();
			if (triggerUpgrade)
			{
				if (obj->hasUpgrade(triggerUpgrade) ||
					(controllingPlayer && controllingPlayer->hasUpgradeComplete(triggerUpgrade)))
				{
					hasLocomotorUpgrade = TRUE;
				}
			}
		}
	}

	LocomotorSetType activeSet =
		(hasLocomotorUpgrade && hasUpgradedMax) ? LOCOMOTORSET_NORMAL_UPGRADED : LOCOMOTORSET_NORMAL;

	BodyDamageType currentDamageType = BODY_PRISTINE;
	const BodyModuleInterface* body = obj->getBodyModule();
	if (body)
	{
		currentDamageType = body->getDamageState();
	}

	Bool hasActiveMax =
		getDisplaySpeedForLocomotorSetAndCondition(thing, activeSet, BODY_PRISTINE, activeMaxDisplaySpeed);

	Bool hasDamagedMax = FALSE;
	if (currentDamageType != BODY_PRISTINE)
	{
		hasDamagedMax =
			getDisplaySpeedForLocomotorSetAndCondition(thing, activeSet, currentDamageType, damagedMaxDisplaySpeed);
	}

	PhysicsBehavior* physics = obj->getPhysics();
	Int currentDisplaySpeed = 0;
	Bool hasCurrentSpeed = FALSE;

	if (physics)
	{
		const Coord3D* vel = physics->getVelocity();
		if (vel)
		{
			Real currentSpeed = sqrt((vel->x * vel->x) + (vel->y * vel->y));
			currentDisplaySpeed = REAL_TO_INT_FLOOR((currentSpeed * LOGICFRAMES_PER_SECOND) + 0.5f);
			hasCurrentSpeed = TRUE;
		}
	}

	if (hasCurrentSpeed && hasActiveMax)
	{
		if (hasLocomotorUpgrade && hasBaseMax && hasUpgradedMax && upgradedMaxDisplaySpeed != baseMaxDisplaySpeed)
		{
			if (hasDamagedMax && damagedMaxDisplaySpeed != activeMaxDisplaySpeed)
			{
				locomotorText.format(
					L"Movement Speed: %d / %d (Base: %d, Upgraded Max: %d with %ls, Damaged Max: %d)",
					currentDisplaySpeed,
					activeMaxDisplaySpeed,
					baseMaxDisplaySpeed,
					upgradedMaxDisplaySpeed,
					triggerUpgradeDisplay.str(),
					damagedMaxDisplaySpeed);
			}
			else
			{
				locomotorText.format(
					L"Movement Speed: %d / %d (Base: %d, Upgraded Max: %d with %ls)",
					currentDisplaySpeed,
					activeMaxDisplaySpeed,
					baseMaxDisplaySpeed,
					upgradedMaxDisplaySpeed,
					triggerUpgradeDisplay.str());
			}
		}
		else
		{
			if (hasDamagedMax && damagedMaxDisplaySpeed != activeMaxDisplaySpeed)
			{
				locomotorText.format(
					L"Movement Speed: %d / %d (Damaged Max: %d)",
					currentDisplaySpeed,
					activeMaxDisplaySpeed,
					damagedMaxDisplaySpeed);
			}
			else
			{
				locomotorText.format(
					L"Movement Speed: %d / %d",
					currentDisplaySpeed,
					activeMaxDisplaySpeed);
			}
		}
	}
}



			UnsignedInt buildLimit = thing->getMaxSimultaneousOfType();
			if (buildLimit > 0)
			{
				Int currentCount = 0;
				const ThingTemplate* tmpl = thing;
				player->countObjectsByThingTemplate(1, &tmpl, false, &currentCount);
				limittext.format(L"Build Limit: %d/%d", currentCount, buildLimit);
			}


			static NameKeyType autoDepositKey = TheNameKeyGenerator->nameToKey("AutoDepositUpdate");
			AutoDepositUpdate* autoDeposit = (AutoDepositUpdate*)obj->findUpdateModule(autoDepositKey);

			if (autoDeposit)
			{
				const AutoDepositUpdateModuleData* autoData = autoDeposit->getAutoDepositUpdateModuleDataForTooltip();

				if (autoData)
				{
					Int baseAmount = autoData->m_depositAmount;
					Int activeBoost = 0;
					Int displayAmount = baseAmount;
					UnicodeString boostUpgradeDisplay = UnicodeString::TheEmptyString;

					Player* controllingPlayer = obj->getControllingPlayer();

					for (std::list<upgradePair>::const_iterator it = autoData->m_upgradeBoost.begin(); it != autoData->m_upgradeBoost.end(); ++it)
					{
						const UpgradeTemplate* boostUpgrade = TheUpgradeCenter->findUpgrade(it->type.c_str());

						if (!boostUpgrade)
							continue;

						if (boostUpgradeDisplay.isEmpty())
						{
							if (boostUpgrade->getDisplayNameLabel().isNotEmpty())
								boostUpgradeDisplay = TheGameText->fetch(boostUpgrade->getDisplayNameLabel().str());
							else
								boostUpgradeDisplay.format(L"%S", it->type.c_str());
						}

						if (controllingPlayer && controllingPlayer->hasUpgradeComplete(boostUpgrade))
						{
							activeBoost = it->amount;
							displayAmount += activeBoost;
							break;
						}
					}

					Int displayBaseAmount = baseAmount;
					Int displayBoostAmount = activeBoost;

					if (g_resourceMultiplierPercent != 100)
					{
						displayAmount = (displayAmount * g_resourceMultiplierPercent) / 100;
						displayBaseAmount = (displayBaseAmount * g_resourceMultiplierPercent) / 100;
						displayBoostAmount = (displayBoostAmount * g_resourceMultiplierPercent) / 100;
					}

					Int seconds = REAL_TO_INT_FLOOR((Real)autoData->m_depositFrame / LOGICFRAMES_PER_SECOND + 0.5f);

					Real incomePerSecond = (seconds > 0) ? ((Real)displayAmount / (Real)seconds) : 0.0f;

					UnicodeString multiplierText = UnicodeString::TheEmptyString;

					if (g_resourceMultiplierPercent != 100)
					{
						multiplierText.format(L" (x%.2f)", (Real)g_resourceMultiplierPercent / 100.0f);
					}

					if (displayBoostAmount > 0 && !boostUpgradeDisplay.isEmpty())
					{
						autoDepositText.format(L"Auto Income: $%d every %d sec (%.1f/sec)%ls (Base: $%d, +$%d with %ls)",
							displayAmount,
							seconds,
							incomePerSecond,
							multiplierText.str(),
							displayBaseAmount,
							displayBoostAmount,
							boostUpgradeDisplay.str());
					}
					else if (!boostUpgradeDisplay.isEmpty() && !autoData->m_upgradeBoost.empty())
					{
						Int potentialBoost = autoData->m_upgradeBoost.front().amount;

						if (g_resourceMultiplierPercent != 100)
							potentialBoost = (potentialBoost * g_resourceMultiplierPercent) / 100;

						autoDepositText.format(L"Auto Income: $%d every %d sec (%.1f/sec)%ls (Base: $%d, +$%d with %ls)",
							displayAmount,
							seconds,
							incomePerSecond,
							multiplierText.str(),
							displayBaseAmount,
							potentialBoost,
							boostUpgradeDisplay.str());
					}
					else
					{
						autoDepositText.format(L"Auto Income: $%d every %d sec (%.1f/sec)%ls",
							displayAmount,
							seconds,
							incomePerSecond,
							multiplierText.str());
					}
				}
			}



			static NameKeyType hackInternetKey = TheNameKeyGenerator->nameToKey("HackInternetAIUpdate");
			HackInternetAIUpdate* hackInternet = (HackInternetAIUpdate*)obj->findUpdateModule(hackInternetKey);

			if (hackInternet)
			{
				Int amount = hackInternet->getRegularCashAmount();
				UnicodeString rankText = L"Regular";

				if (obj->getVeterancyLevel() == LEVEL_VETERAN)
				{
					amount = hackInternet->getVeteranCashAmount();
					rankText = L"Veteran";
				}
				else if (obj->getVeterancyLevel() == LEVEL_ELITE)
				{
					amount = hackInternet->getEliteCashAmount();
					rankText = L"Elite";
				}
				else if (obj->getVeterancyLevel() == LEVEL_HEROIC)
				{
					amount = hackInternet->getHeroicCashAmount();
					rankText = L"Heroic";
				}

				if (g_resourceMultiplierPercent != 100)
					amount = (amount * g_resourceMultiplierPercent) / 100;

				Int seconds = REAL_TO_INT_FLOOR((Real)hackInternet->getCashUpdateDelay() / LOGICFRAMES_PER_SECOND + 0.5f);
				Real incomePerSecond = (seconds > 0) ? ((Real)amount / (Real)seconds) : 0.0f;

				UnicodeString multiplierText = UnicodeString::TheEmptyString;
				if (g_resourceMultiplierPercent != 100)
					multiplierText.format(L" (x%.2f)", (Real)g_resourceMultiplierPercent / 100.0f);

				hackInternetText.format(L"Hack Income: $%d every %d sec (%.1f/sec) (%ls)%ls",
					amount,
					seconds,
					incomePerSecond,
					rankText.str(),
					multiplierText.str());
			}


			if (obj && thing->isKindOf(KINDOF_FS_INTERNET_CENTER))
			{
				Int hackerCount = 0;
				Int totalAmount = 0;
				Int delayFrames = 0;

				if (getInternetCenterContainedHackIncomeForTooltip(obj, hackerCount, totalAmount, delayFrames))
				{
					Int seconds = REAL_TO_INT_FLOOR((Real)delayFrames / LOGICFRAMES_PER_SECOND + 0.5f);
					Real incomePerSecond = (seconds > 0) ? ((Real)totalAmount / (Real)seconds) : 0.0f;

					UnicodeString multiplierText = UnicodeString::TheEmptyString;
					if (g_resourceMultiplierPercent != 100)
						multiplierText.format(L" (x%.2f)", (Real)g_resourceMultiplierPercent / 100.0f);

					if (hackerCount == 1)
					{
						internetCenterHackText.format(L"Contained Hack Income: $%d every %d sec (%.1f/sec)%ls (%d Hacker)",
							totalAmount,
							seconds,
							incomePerSecond,
							multiplierText.str(),
							hackerCount);
					}
					else
					{
						internetCenterHackText.format(L"Contained Hack Income: $%d every %d sec (%.1f/sec)%ls (%d Hackers)",
							totalAmount,
							seconds,
							incomePerSecond,
							multiplierText.str(),
							hackerCount);
					}
				}
			}

			if (getSupplyDropZoneIncomeForTooltip(thing, obj, player, supplyDropText))
			{
			}

	if (!healthText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(healthText); }
	if (!powerText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(powerText); }
	if (!shroudText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(shroudText); }
	if (!stealthDetectText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(stealthDetectText); }
	if (!locomotorText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(locomotorText); }
	if (!limittext.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(limittext); }
	if (!autoDepositText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(autoDepositText); }
	if (!hackInternetText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(hackInternetText); }
	if (!internetCenterHackText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(internetCenterHackText); }
	if (!supplyDropText.isEmpty()) { if (!infoText.isEmpty()) infoText.concat(L"\n"); infoText.concat(supplyDropText); }

	GameWindow* root = m_buildToolTipLayout->getFirstWindow();

	GameWindow* titleWin = TheWindowManager->winGetWindowFromId(root, TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextName"));
	GameWindow* descWin = TheWindowManager->winGetWindowFromId(root, TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextDescription"));
	GameWindow* costWin = TheWindowManager->winGetWindowFromId(root, TheNameKeyGenerator->nameToKey("ControlBarPopupDescription.wnd:StaticTextCost"));

	if (titleWin)
	{
		titleWin->winHide(FALSE);
		GadgetStaticTextSetText(titleWin, name);
	}

	if (descWin)
	{
		descWin->winHide(FALSE);
		GadgetStaticTextSetText(descWin, descrip);
	}

	if (costWin)
	{
		costWin->winHide(FALSE);
		GadgetStaticTextSetText(costWin, infoText);
	}


	{
		static NameKeyType winNamekey = TheNameKeyGenerator->nameToKey("ControlBar.wnd:BackgroundMarker");
		static ICoord2D lastOffset = { 0, 0 };
		if (s_tooltipSizesInitialized)
		{
			lastOffset.x = 0;
			lastOffset.y = 0;
		}


		GameWindow* root = m_buildToolTipLayout->getFirstWindow();
		if (!root || !costWin || !descWin)
			return;

		ICoord2D size, newSize, pos;
		Int costDiffSize = 0;
		Int diffSize = 0;

		DisplayString* tempDString = TheDisplayStringManager->newDisplayString();

		costWin->winGetSize(&size.x, &size.y);
		tempDString->setFont(costWin->winGetFont());
		tempDString->setWordWrap(size.x - 10);
		tempDString->setText(infoText);
		tempDString->getSize(&newSize.x, &newSize.y);

		costDiffSize = newSize.y - size.y;
		if (costDiffSize < 0)
			costDiffSize = 0;

		costWin->winSetSize(size.x, size.y + costDiffSize);
		GadgetStaticTextSetText(costWin, infoText);

		ICoord2D descPos;
		descWin->winGetPosition(&descPos.x, &descPos.y);
		descWin->winSetPosition(descPos.x, descPos.y + costDiffSize);

		descWin->winGetSize(&size.x, &size.y);
		tempDString->setFont(descWin->winGetFont());
		tempDString->setWordWrap(size.x - 10);
		tempDString->setText(descrip);
		tempDString->getSize(&newSize.x, &newSize.y);

		diffSize = newSize.y - size.y;

		TheDisplayStringManager->freeDisplayString(tempDString);
		tempDString = nullptr;

		root->winGetSize(&size.x, &size.y);

		if (size.y + diffSize < 64)
			diffSize = 64 - size.y;

		root->winSetSize(size.x, size.y + costDiffSize + diffSize);
		root->winGetPosition(&pos.x, &pos.y);

		GameWindow* marker = TheWindowManager->winGetWindowFromId(nullptr, winNamekey);
		static ICoord2D basePos;
		if (!marker)
			return;

		getBackgroundMarkerPos(&basePos.x, &basePos.y);

		ICoord2D curPos, offset;
		marker->winGetScreenPosition(&curPos.x, &curPos.y);

		offset.x = curPos.x - basePos.x;
		offset.y = curPos.y - basePos.y;

		root->winSetPosition(pos.x, (pos.y - costDiffSize - diffSize) + (offset.y - lastOffset.y));

		lastOffset.x = offset.x;
		lastOffset.y = offset.y;

		descWin->winGetSize(&size.x, &size.y);
		descWin->winSetSize(size.x, size.y + diffSize);
		GadgetStaticTextSetText(descWin, descrip);
	}

	m_buildToolTipLayout->hide(FALSE);
}


// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ControlBar::hideBuildTooltipLayout()
{
	if(theAnimateWindowManager && theAnimateWindowManager->isReversed())
		return;
	if(useAnimation && theAnimateWindowManager && TheGlobalData->m_animateWindows)
		theAnimateWindowManager->reverseAnimateWindow();
	else
		deleteBuildTooltipLayout();

}

void ControlBar::deleteBuildTooltipLayout()
{
	m_showBuildToolTipLayout = FALSE;
	prevWindow= nullptr;
	m_buildToolTipLayout->hide(TRUE);
//	if(!m_buildToolTipLayout)
//		return;
//
//	m_buildToolTipLayout->destroyWindows();
//	deleteInstance(m_buildToolTipLayout);
//	m_buildToolTipLayout = nullptr;

	delete theAnimateWindowManager;
	theAnimateWindowManager = nullptr;

}

