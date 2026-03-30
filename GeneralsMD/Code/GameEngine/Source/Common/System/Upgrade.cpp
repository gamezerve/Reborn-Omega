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

// FILE: Upgrade.cpp //////////////////////////////////////////////////////////////////////////////
// Author: Colin Day, March 2002
// Desc:   Upgrade system for players
///////////////////////////////////////////////////////////////////////////////////////////////////

// USER INCLUDES //////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include <windows.h> // Reborn: for CreateDirectoryA
#include <stdio.h>   // Reborn: for file writing
#include <set>
#include <map>

#define DEFINE_VETERANCY_NAMES
#include "Common/Upgrade.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "Common/ThingTemplate.h"
#include "Common/ThingFactory.h"
#include "GameClient/InGameUI.h"
#include "GameClient/Image.h"


const char *const TheUpgradeTypeNames[] =
{
	"PLAYER",
	"OBJECT",
	nullptr
};
static_assert(ARRAY_SIZE(TheUpgradeTypeNames) == NUM_UPGRADE_TYPES + 1, "Incorrect array size");

// PUBLIC /////////////////////////////////////////////////////////////////////////////////////////
class UpgradeCenter *TheUpgradeCenter = nullptr;

static AsciiString g_currentThingTemplateUpgradeCapture;
static Bool g_isThingTemplateUpgradeCaptureActive = FALSE;
static Bool g_upgradeReportDirty = FALSE;
static std::set<AsciiString> g_knownUpgradeNames;

struct ThingTemplateUpgradeReportEntry
{
	std::set<AsciiString> cameos;
	std::set<AsciiString> refs;
};

static std::map<AsciiString, ThingTemplateUpgradeReportEntry> g_thingTemplateUpgradeReport;



void BeginThingTemplateUpgradeCapture(const char* thingName)
{
	if (thingName == nullptr || thingName[0] == 0)
		return;

	g_currentThingTemplateUpgradeCapture = thingName;
	g_isThingTemplateUpgradeCaptureActive = TRUE;
}

void EndThingTemplateUpgradeCapture()
{
	g_currentThingTemplateUpgradeCapture.clear();
	g_isThingTemplateUpgradeCaptureActive = FALSE;
}

Bool IsKnownUpgradeName(const char* token)
{
	if (token == nullptr || token[0] == 0)
		return FALSE;

	return g_knownUpgradeNames.find(AsciiString(token)) != g_knownUpgradeNames.end();
}

static Bool LooksLikeUpgradeName(const char* token)
{
	if (token == nullptr || token[0] == 0)
		return FALSE;

	if (strnicmp(token, "Upgrade_", 8) == 0)
	{
		unsigned char next = (unsigned char)token[8];
		if (next >= '0' && next <= '9')
			return FALSE;

		return TRUE;
	}

	return strstr(token, "_Upgrade_") != nullptr;
}


static AsciiString g_currentThingTemplateUpgradeField;

static Bool ShouldIgnoreUpgradeReferenceForCurrentField()
{
	if (g_currentThingTemplateUpgradeField.isEmpty())
		return FALSE;

	if (stricmp(g_currentThingTemplateUpgradeField.str(), "UpgradeToRemove") == 0)
		return TRUE;

	if (stricmp(g_currentThingTemplateUpgradeField.str(), "UpgradeToRemoveOnSell") == 0)
		return TRUE;

	if (stricmp(g_currentThingTemplateUpgradeField.str(), "UpgradeToGrant") == 0)
		return TRUE;

	return FALSE;
}

void RecordThingTemplateUpgradeToken(const char* token)
{
	if (!g_isThingTemplateUpgradeCaptureActive)
		return;
	if (g_currentThingTemplateUpgradeCapture.isEmpty())
		return;
	if (token == nullptr || token[0] == 0)
		return;
	if (!LooksLikeUpgradeName(token))
		return;
	if (ShouldIgnoreUpgradeReferenceForCurrentField())
		return;


	g_thingTemplateUpgradeReport[g_currentThingTemplateUpgradeCapture].refs.insert(AsciiString(token));
	g_upgradeReportDirty = TRUE;
}

void RecordThingTemplateUpgradeCameo(const char* thingName, const char* upgradeName)
{
	if (thingName == nullptr || thingName[0] == 0 || upgradeName == nullptr || upgradeName[0] == 0)
		return;

	g_thingTemplateUpgradeReport[AsciiString(thingName)].cameos.insert(AsciiString(upgradeName));
	g_thingTemplateUpgradeReport[AsciiString(thingName)].refs.insert(AsciiString(upgradeName));
	g_upgradeReportDirty = TRUE;
}


void SetCurrentThingTemplateUpgradeField(const char* fieldName)
{
	if (fieldName && fieldName[0])
		g_currentThingTemplateUpgradeField = fieldName;
	else
		g_currentThingTemplateUpgradeField.clear();
}

void ClearCurrentThingTemplateUpgradeField()
{
	g_currentThingTemplateUpgradeField.clear();
}


static Bool IsUpgradeCameoException(const char* upgradeName)
{
	if (upgradeName == nullptr || upgradeName[0] == 0)
		return FALSE;

	static const std::set<AsciiString> s_exceptions =
	{
		"Upgrade_CostReduction",
		"Upgrade_GLARadar",
		"Upgrade_BecomeRealGLASupplyStash",
		"Upgrade_BecomeRealGLACommandCenter",
		"Upgrade_BecomeRealGLABlackMarket",
		"Upgrade_BecomeRealGLABarracks",
		"Upgrade_BecomeRealGLAArmsDealer",
		"Upgrade_Veterancy_ELITE",
		"Upgrade_Veterancy_HEROIC",
		"Upgrade_ChinaEMPMines",
		"Upgrade_ChinaMines",
		"Upgrade_AmericaRangerTaunt",
		"Upgrade_AmericaMOAB",
		"Upgrade_AmericaRadar",
		"Upgrade_GLAWorkerFakeCommandSet",
		"Upgrade_GLAWorkerRealCommandSet"
	};

	return s_exceptions.find(AsciiString(upgradeName)) != s_exceptions.end();
}


static Bool IsTrainabilityRelatedUpgradeReferenceException(const ThingTemplate* thing, const char* upgradeName)
{
	if (thing == nullptr || upgradeName == nullptr || upgradeName[0] == 0)
		return FALSE;

	static const std::set<AsciiString> s_trainabilityRelatedExceptions =
	{
		"Upgrade_AmericaAdvancedTraining",
	};

	return s_trainabilityRelatedExceptions.find(AsciiString(upgradeName)) != s_trainabilityRelatedExceptions.end();
}

static const char* GetUpgradeCameoExceptionReason(const char* upgradeName)
{
	if (upgradeName == nullptr || upgradeName[0] == 0)
		return nullptr;

	if (IsUpgradeCameoException(upgradeName))
		return "placeholder upgrade (Excluded)";

	return nullptr;
}

static const char* GetTrainabilityRelatedUpgradeReferenceExceptionReason(const ThingTemplate* thing, const char* upgradeName)
{
	if (upgradeName == nullptr || upgradeName[0] == 0)
		return nullptr;

	if (IsTrainabilityRelatedUpgradeReferenceException(thing, upgradeName))
	{
		if (thing && !thing->isTrainable())
			return "object is not trainable (Excluded)";

		return "trainability-related upgrade";
	}

	return nullptr;
}

static const char* GetUpgradeReferenceExceptionReason(const ThingTemplate* thing, const char* upgradeName)
{
	const char* reason = GetUpgradeCameoExceptionReason(upgradeName);
	if (reason != nullptr)
		return reason;

	reason = GetTrainabilityRelatedUpgradeReferenceExceptionReason(thing, upgradeName);
	if (reason != nullptr)
		return reason;

	return nullptr;
}

void FlushThingTemplateUpgradeReport()
{
	if (!g_upgradeReportDirty)
		return;

	CreateDirectoryA("RebornStatus", nullptr);

	FILE* fp = fopen("RebornStatus\\ThingTemplateUpgradeRefs.txt", "wt");
	if (fp == nullptr)
		return;

	FILE* overflowFp = fopen("RebornStatus\\ThingTemplateUpgradeRefs_TooManyEffectiveReferences.txt", "wt");

	for (std::map<AsciiString, ThingTemplateUpgradeReportEntry>::const_iterator it = g_thingTemplateUpgradeReport.begin(); it != g_thingTemplateUpgradeReport.end(); ++it)
	{
		const ThingTemplate* thing = TheThingFactory ? TheThingFactory->findTemplate(it->first.str()) : nullptr;
		Int cameoCount = (Int)it->second.cameos.size();
		Int totalRefCount = (Int)it->second.refs.size();
		Int exceptionRefCount = 0;
		Int effectiveRefCount = 0;

		AsciiString missing;

		for (std::set<AsciiString>::const_iterator r = it->second.refs.begin(); r != it->second.refs.end(); ++r)
		{
			const char* exceptionReason = GetUpgradeReferenceExceptionReason(thing, r->str());
			if (exceptionReason != nullptr)
			{
				exceptionRefCount++;
				continue;
			}

			effectiveRefCount++;

			if (it->second.cameos.find(*r) == it->second.cameos.end())
			{
				if (!missing.isEmpty())
					missing.concat(", ");
				missing.concat(*r);
			}
		}

		if (!missing.isEmpty())
		{
			DEBUG_ASSERTCRASH(false, ("ThingTemplate '%s' has upgrade references missing from UpgradeCameos: %s",
				it->first.str(), missing.str()));
		}

		if (effectiveRefCount > 7)
		{
			DEBUG_ASSERTCRASH(false, ("ThingTemplate '%s' has %d effective upgrade references which exceeds the supported UpgradeCameo limit of 7",
				it->first.str(), effectiveRefCount));

			if (overflowFp != nullptr)
			{
				fprintf(overflowFp, "Object=%s\n", it->first.str());
				fprintf(overflowFp, "  UpgradeCameoCount=%d\n", cameoCount);
				fprintf(overflowFp, "  AllUpgradeReferencesCount=%d\n", totalRefCount);
				fprintf(overflowFp, "  AllUpgradeReferencesExceptionCount=%d\n", exceptionRefCount);
				fprintf(overflowFp, "  AllUpgradeReferencesEffectiveCount=%d\n", effectiveRefCount);

				fprintf(overflowFp, "  UpgradeCameos:\n");
				for (std::set<AsciiString>::const_iterator c = it->second.cameos.begin(); c != it->second.cameos.end(); ++c)
					fprintf(overflowFp, "    %s\n", c->str());

				fprintf(overflowFp, "  AllUpgradeReferences:\n");
				for (std::set<AsciiString>::const_iterator r = it->second.refs.begin(); r != it->second.refs.end(); ++r)
				{
					const char* exceptionReason = GetUpgradeReferenceExceptionReason(thing, r->str());
					if (exceptionReason != nullptr)
						fprintf(overflowFp, "    %s (%s)\n", r->str(), exceptionReason);
					else
						fprintf(overflowFp, "    %s\n", r->str());
				}

				fprintf(overflowFp, "\n");
			}
		}

		fprintf(fp, "Object=%s\n", it->first.str());
		fprintf(fp, "  UpgradeCameoCount=%d\n", cameoCount);
		fprintf(fp, "  AllUpgradeReferencesCount=%d\n", totalRefCount);
		fprintf(fp, "  AllUpgradeReferencesExceptionCount=%d\n", exceptionRefCount);
		fprintf(fp, "  AllUpgradeReferencesEffectiveCount=%d\n", effectiveRefCount);

		fprintf(fp, "  UpgradeCameos:\n");
		for (std::set<AsciiString>::const_iterator c = it->second.cameos.begin(); c != it->second.cameos.end(); ++c)
			fprintf(fp, "    %s\n", c->str());

		fprintf(fp, "  AllUpgradeReferences:\n");
		for (std::set<AsciiString>::const_iterator r = it->second.refs.begin(); r != it->second.refs.end(); ++r)
		{
			const char* exceptionReason = GetUpgradeReferenceExceptionReason(thing, r->str());
			if (exceptionReason != nullptr)
				fprintf(fp, "    %s (%s)\n", r->str(), exceptionReason);
			else
				fprintf(fp, "    %s\n", r->str());
		}

		fprintf(fp, "\n");
	}

	fclose(fp);

	if (overflowFp != nullptr)
		fclose(overflowFp);

	g_upgradeReportDirty = FALSE;
}

static void WriteUpgradeListFile(const UpgradeTemplate* head) // Reborn: writes a text file with the list of all upgrades
{
	CreateDirectoryA("RebornStatus", nullptr);

	FILE* fp = fopen("RebornStatus\\UpgradeList.txt", "wt");
	if (!fp)
		return;

	int count = 0;
	for (const UpgradeTemplate* upgrade = head; upgrade; upgrade = upgrade->friend_getNext())
	{
		const char* name = upgrade->getUpgradeName().str();
		if (name && *name)
		{
			fprintf(fp, "%s\n", name);
			++count;
		}
	}

	fprintf(fp, "\nTotalUpgrades=%d\n", count);
	fclose(fp);
}



///////////////////////////////////////////////////////////////////////////////////////////////////
// UPGRADE ////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Upgrade::Upgrade( const UpgradeTemplate *upgradeTemplate )
{

	m_template = upgradeTemplate;
	m_status = UPGRADE_STATUS_INVALID;
	m_next = nullptr;
	m_prev = nullptr;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
Upgrade::~Upgrade()
{

}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void Upgrade::crc( Xfer *xfer )
{

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void Upgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// status
	xfer->xferUser( &m_status, sizeof( UpgradeStatusType ) );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void Upgrade::loadPostProcess()
{

}

///////////////////////////////////////////////////////////////////////////////////////////////////
// UPGRADE TEMPLATE ///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
const FieldParse UpgradeTemplate::m_upgradeFieldParseTable[] =
{

	{ "DisplayName",				INI::parseAsciiString,		nullptr, offsetof( UpgradeTemplate, m_displayNameLabel ) },
	{ "Type",								INI::parseIndexList,			TheUpgradeTypeNames, offsetof( UpgradeTemplate, m_type ) },
	{ "BuildTime",					INI::parseReal,						nullptr, offsetof( UpgradeTemplate, m_buildTime ) },
	{ "BuildCost",					INI::parseInt,						nullptr, offsetof( UpgradeTemplate, m_cost ) },
	{ "ButtonImage",				INI::parseAsciiString,		nullptr, offsetof( UpgradeTemplate, m_buttonImageName ) },
	{ "ResearchSound",			INI::parseAudioEventRTS,	nullptr, offsetof( UpgradeTemplate, m_researchSound ) },
	{ "UnitSpecificSound",	INI::parseAudioEventRTS,	nullptr, offsetof( UpgradeTemplate, m_unitSpecificSound ) },
	{ "AcademyClassify",		INI::parseIndexList,			TheAcademyClassificationTypeNames, offsetof( UpgradeTemplate, m_academyClassificationType ) },
	{ nullptr,						nullptr,												 nullptr, 0 }

};

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeTemplate::UpgradeTemplate()
{
	m_cost = 0;
	m_type = UPGRADE_TYPE_PLAYER;
	m_nameKey = NAMEKEY_INVALID;
	m_buildTime = 0.0f;
	m_next = nullptr;
	m_prev = nullptr;
	m_buttonImage = nullptr;
	m_academyClassificationType = ACT_NONE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeTemplate::~UpgradeTemplate()
{

}

//-------------------------------------------------------------------------------------------------
/** Calculate the time it takes (in logic frames) for a player to build this UpgradeTemplate */
//-------------------------------------------------------------------------------------------------
Int UpgradeTemplate::calcTimeToBuild( Player *player ) const
{
#if defined(RTS_DEBUG) || defined(_ALLOW_DEBUG_CHEATS_IN_RELEASE)
	if( player->buildsInstantly() )
	{
		return 1;
	}
#endif

	///@todo modify this by power state of player
	return m_buildTime * LOGICFRAMES_PER_SECOND;

}

//-------------------------------------------------------------------------------------------------
/** Calculate the cost takes this player to build this upgrade */
//-------------------------------------------------------------------------------------------------
Int UpgradeTemplate::calcCostToBuild( Player *player ) const
{

	///@todo modify this by any player handicaps
	return m_cost;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
static AsciiString getVetUpgradeName(VeterancyLevel v)
{
	AsciiString tmp;
	tmp = "Upgrade_Veterancy_";
	tmp.concat(TheVeterancyNames[v]);
	return tmp;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void UpgradeTemplate::friend_makeVeterancyUpgrade(VeterancyLevel v)
{
	m_type = UPGRADE_TYPE_OBJECT;	// veterancy "upgrades" are always per-object, not per-player
	m_name = getVetUpgradeName(v);
	m_nameKey = TheNameKeyGenerator->nameToKey( m_name );
	m_displayNameLabel.clear();	// should never be displayed
	m_buildTime = 0.0f;
	m_cost = 0.0f;
	// leave this alone.
	//m_upgradeMask = ???;
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void UpgradeTemplate::cacheButtonImage()
{
	if( m_buttonImageName.isNotEmpty() )
	{
		m_buttonImage = TheMappedImageCollection->findImageByName( m_buttonImageName );
		DEBUG_ASSERTCRASH( m_buttonImage, ("UpgradeTemplate: %s is looking for button image %s but can't find it. Skipping...", m_name.str(), m_buttonImageName.str() ) );
		m_buttonImageName.clear();	// we're done with this, so nuke it
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// UPGRADE CENTER /////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeCenter::UpgradeCenter()
{

	m_upgradeList = nullptr;
	m_nextTemplateMaskBit = 0;
	buttonImagesCached = FALSE;

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
UpgradeCenter::~UpgradeCenter()
{

	// delete all the upgrades loaded from the INI database
	UpgradeTemplate *next;
	while( m_upgradeList )
	{

		// get next
		next = m_upgradeList->friend_getNext();

		// delete head of list
		deleteInstance(m_upgradeList);

		// set head to next element
		m_upgradeList = next;

	}

}

//-------------------------------------------------------------------------------------------------
/** Upgrade center initialization */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::init()
{
	UpgradeTemplate* up;

	// name will be overridden by friend_makeVeterancyUpgrade

// no, there ISN'T an upgrade for this one...
//up = newUpgrade("");
//up->friend_makeVeterancyUpgrade(LEVEL_REGULAR);

	up = newUpgrade("");
	up->friend_makeVeterancyUpgrade(LEVEL_VETERAN);

	up = newUpgrade("");
	up->friend_makeVeterancyUpgrade(LEVEL_ELITE);

	up = newUpgrade("");
	up->friend_makeVeterancyUpgrade(LEVEL_HEROIC);

}

//-------------------------------------------------------------------------------------------------
/** Upgrade center system reset */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::reset()
{
	if (TheMappedImageCollection && !buttonImagesCached)
	{
		UpgradeTemplate* upgrade;
		for (upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext())
		{
			upgrade->cacheButtonImage();
		}
		buttonImagesCached = TRUE;
	}

#ifdef RTS_DEBUG
	WriteUpgradeListFile(m_upgradeList); // Reborn: write out the list of upgrades to a file for debugging purposes
#endif

#ifdef RTS_DEBUG
	for (UpgradeTemplate* upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext())
	{
		g_knownUpgradeNames.insert(upgrade->getUpgradeName());
	}

	FlushThingTemplateUpgradeReport();
#endif
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by veterancy level */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findVeterancyUpgrade( VeterancyLevel level ) const
{
	AsciiString tmp = getVetUpgradeName(level);
	return findUpgrade(tmp);
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name key */
//-------------------------------------------------------------------------------------------------
UpgradeTemplate *UpgradeCenter::findNonConstUpgradeByKey( NameKeyType key )
{
	UpgradeTemplate *upgrade;

	// search list
	for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		if( upgrade->getUpgradeNameKey() == key )
			return upgrade;

	// item not found
	return nullptr;

}

// ------------------------------------------------------------------------------------------------
/** Return the first upgrade template */
// ------------------------------------------------------------------------------------------------
UpgradeTemplate *UpgradeCenter::firstUpgradeTemplate()
{

	return m_upgradeList;

}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name key */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findUpgradeByKey( NameKeyType key ) const
{
	const UpgradeTemplate *upgrade;

	// search list
	for( upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		if( upgrade->getUpgradeNameKey() == key )
			return upgrade;

	// item not found
	return nullptr;
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findUpgrade( const AsciiString& name ) const
{
	return findUpgradeByKey( TheNameKeyGenerator->nameToKey( name ) );
}

//-------------------------------------------------------------------------------------------------
/** Find upgrade by name */
//-------------------------------------------------------------------------------------------------
const UpgradeTemplate *UpgradeCenter::findUpgrade( const char* name ) const
{
	return findUpgradeByKey( TheNameKeyGenerator->nameToKey( name ) );
}

//-------------------------------------------------------------------------------------------------
/** Allocate a new upgrade template */
//-------------------------------------------------------------------------------------------------
UpgradeTemplate *UpgradeCenter::newUpgrade( const AsciiString& name )
{
	UpgradeTemplate *newUpgrade = newInstance(UpgradeTemplate);

	// copy data from the default upgrade
	const UpgradeTemplate *defaultUpgrade = findUpgrade( "DefaultUpgrade" );
	if( defaultUpgrade )
		*newUpgrade = *defaultUpgrade;

	// assign name and starting data
	newUpgrade->setUpgradeName( name );
	newUpgrade->setUpgradeNameKey( TheNameKeyGenerator->nameToKey( name ) );

	// Make a unique bitmask for this new template by keeping track of what bits have been assigned
	// damn MSFT! proper ANSI syntax for a proper 64-bit constant is "1LL", but MSVC doesn't recognize it
	UpgradeMaskType newMask;
	newMask.set( m_nextTemplateMaskBit );
	//Int64 newMask = 1i64 << m_nextTemplateMaskBit;
	m_nextTemplateMaskBit++;
	DEBUG_ASSERTCRASH( m_nextTemplateMaskBit < UPGRADE_MAX_COUNT, ("Can't have over %d types of Upgrades and have a Bitfield function.", UPGRADE_MAX_COUNT) );
	newUpgrade->friend_setUpgradeMask( newMask );

	// link upgrade
	linkUpgrade( newUpgrade );

	// return new upgrade
	return newUpgrade;

}

//-------------------------------------------------------------------------------------------------
/** Link an upgrade to our list */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::linkUpgrade( UpgradeTemplate *upgrade )
{

	// sanity
	if( upgrade == nullptr )
		return;

	// link
	upgrade->friend_setPrev( nullptr );
	upgrade->friend_setNext( m_upgradeList );
	if( m_upgradeList )
		m_upgradeList->friend_setPrev( upgrade );
	m_upgradeList = upgrade;

}

//-------------------------------------------------------------------------------------------------
/** Unlink an upgrade from our list */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::unlinkUpgrade( UpgradeTemplate *upgrade )
{

	// sanity
	if( upgrade == nullptr )
		return;

	if( upgrade->friend_getNext() )
		upgrade->friend_getNext()->friend_setPrev( upgrade->friend_getPrev() );
	if( upgrade->friend_getPrev() )
		upgrade->friend_getPrev()->friend_setNext( upgrade->friend_getNext() );
	else
		m_upgradeList = upgrade->friend_getNext();

}

//-------------------------------------------------------------------------------------------------
/** does this player have all the necessary things to make this upgrade */
//-------------------------------------------------------------------------------------------------
Bool UpgradeCenter::canAffordUpgrade( Player *player, const UpgradeTemplate *upgradeTemplate, Bool displayReason ) const
{

	// sanity
	if( player == nullptr || upgradeTemplate == nullptr )
		return FALSE;

	// money check
	Money *money = player->getMoney();
	if( money->countMoney() < upgradeTemplate->calcCostToBuild( player ) )
	{
		//Post reason why we can't make upgrade!
		if( displayReason )
		{
			TheInGameUI->message( "GUI:NotEnoughMoneyToUpgrade" );
		}
		return FALSE;
	}

	/// @todo maybe have prereq checks for upgrades???

	return TRUE;  // all is well

}

//-------------------------------------------------------------------------------------------------
/** generate a list of upgrade names for WorldBuilder */
//-------------------------------------------------------------------------------------------------
std::vector<AsciiString> UpgradeCenter::getUpgradeNames() const
{
	std::vector<AsciiString> upgradeNames;

	for( UpgradeTemplate *upgrade = m_upgradeList; upgrade; upgrade = upgrade->friend_getNext() )
		upgradeNames.push_back(upgrade->getUpgradeName());

	return upgradeNames;

}

//-------------------------------------------------------------------------------------------------
/** Parse an upgrade definition */
//-------------------------------------------------------------------------------------------------
void UpgradeCenter::parseUpgradeDefinition(INI* ini)
{
	const char* name = ini->getNextToken();

	UpgradeTemplate* upgrade = TheUpgradeCenter->findNonConstUpgradeByKey(NAMEKEY(name));
	if (upgrade == nullptr)
	{
		upgrade = TheUpgradeCenter->newUpgrade(name);
	}

	DEBUG_ASSERTCRASH(upgrade, ("parseUpgradeDefinition: Unable to allocate upgrade '%s'", name));

	g_knownUpgradeNames.insert(AsciiString(name));

	ini->initFromINI(upgrade, upgrade->getFieldParse());
}

