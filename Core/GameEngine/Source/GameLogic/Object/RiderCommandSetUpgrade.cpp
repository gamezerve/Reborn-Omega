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
//																																						
//  (c) 2001-2003 Electronic Arts Inc.																				
//																																						
////////////////////////////////////////////////////////////////////////////////

// FILE: RiderCommandSetUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Gamezerve, September 2026
// Desc: UpgradeModule that sets a new override string for Rider Command Set look ups
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/Player.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"

#include "GameLogic/Object.h"
#include "GameLogic/Module/RiderChangeContain.h"
#include "GameLogic/Module/RiderCommandSetUpgrade.h"

//-------------------------------------------------------------------------------------------------
RiderCommandSetUpgradeModuleData::RiderCommandSetUpgradeModuleData()
{
	m_applicableObjects.clear();
	m_applicableRiders.clear();
	m_commandSetMappings.clear();
}

//-------------------------------------------------------------------------------------------------
void RiderCommandSetUpgradeModuleData::parseApplicableName(
	INI* ini,
	void* instance,
	void* store,
	const void* /*userData*/)
{
	AsciiStringList* names = static_cast<AsciiStringList*>(store);

	AsciiString name;
	name = ini->getNextToken();

	names->push_back(name);
}

//-------------------------------------------------------------------------------------------------
void RiderCommandSetUpgradeModuleData::parseCommandSetMapping(
	INI* ini,
	void* instance,
	void* store,
	const void* /*userData*/)
{
	std::vector<RiderCommandSetMapping>* mappings =
		static_cast<std::vector<RiderCommandSetMapping>*>(store);

	RiderCommandSetMapping mapping;

	mapping.m_sourceCommandSet = ini->getNextToken();
	mapping.m_targetCommandSet = ini->getNextToken();

	mappings->push_back(mapping);
}

//-------------------------------------------------------------------------------------------------
void RiderCommandSetUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	UpgradeModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{
			"AppliesToObject",
			parseApplicableName,
			nullptr,
			offsetof(
				RiderCommandSetUpgradeModuleData,
				m_applicableObjects)
		},

		{
			"AppliesToRider",
			parseApplicableName,
			nullptr,
			offsetof(
				RiderCommandSetUpgradeModuleData,
				m_applicableRiders)
		},

		{
			"CommandSetMapping",
			parseCommandSetMapping,
			nullptr,
			offsetof(
				RiderCommandSetUpgradeModuleData,
				m_commandSetMappings)
		},

		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
static Bool containsTemplateName(
	const AsciiStringList& names,
	const ThingTemplate* thingTemplate)
{
	if (!thingTemplate)
		return FALSE;

	const AsciiString& templateName = thingTemplate->getName();

	for (AsciiStringList::const_iterator it = names.begin();
		it != names.end();
		++it)
	{
		if (it->compareNoCase(templateName) == 0)
			return TRUE;
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
RiderCommandSetUpgrade::RiderCommandSetUpgrade(
	Thing* thing,
	const ModuleData* moduleData) :
	UpgradeModule(thing, moduleData)
{
}

//-------------------------------------------------------------------------------------------------
RiderCommandSetUpgrade::~RiderCommandSetUpgrade()
{
}

//-------------------------------------------------------------------------------------------------
Bool RiderCommandSetUpgrade::resolveCommandSet(
	const Object* object,
	const Object* rider,
	const AsciiString& sourceCommandSet,
	AsciiString& targetCommandSet) const
{
	if (!object || !rider)
		return FALSE;

	const RiderCommandSetUpgradeModuleData* data =
		getRiderCommandSetUpgradeModuleData();

	if (!data)
		return FALSE;

	//
	// Do not use isAlreadyUpgraded() here.
	//
	// UpgradeMux calls upgradeImplementation() before setting
	// m_upgradeExecuted to TRUE, so determine activation from the
	// actual player/object upgrade masks instead.
	//
	const Player* player = object->getControllingPlayer();

	if (!player)
		return FALSE;

	UpgradeMaskType upgradeMask =
		player->getCompletedUpgradeMask();

	upgradeMask.set(
		object->getObjectCompletedUpgradeMask());

	if (!testUpgradeConditions(upgradeMask))
		return FALSE;

	const Bool hasObjectRestrictions =
		!data->m_applicableObjects.empty();

	const Bool hasRiderRestrictions =
		!data->m_applicableRiders.empty();

	//
	// No restrictions means this module applies to every rider/object
	// using the module.
	//
	Bool applicable =
		!hasObjectRestrictions &&
		!hasRiderRestrictions;

	if (hasObjectRestrictions &&
		containsTemplateName(
			data->m_applicableObjects,
			object->getTemplate()))
	{
		applicable = TRUE;
	}

	if (hasRiderRestrictions &&
		containsTemplateName(
			data->m_applicableRiders,
			rider->getTemplate()))
	{
		applicable = TRUE;
	}

	if (!applicable)
		return FALSE;

	for (std::vector<RiderCommandSetMapping>::const_iterator it =
		data->m_commandSetMappings.begin();
		it != data->m_commandSetMappings.end();
		++it)
	{
		if (it->m_sourceCommandSet.compareNoCase(sourceCommandSet) == 0)
		{
			targetCommandSet = it->m_targetCommandSet;
			return TRUE;
		}
	}

	return FALSE;
}

//-------------------------------------------------------------------------------------------------
void RiderCommandSetUpgrade::upgradeImplementation()
{
	Object* object = getObject();

	if (!object)
		return;

	ContainModuleInterface* contain = object->getContain();

	if (!contain || !contain->isRiderChangeContain())
		return;

	RiderChangeContain* riderContain =
		static_cast<RiderChangeContain*>(contain);

	riderContain->refreshCommandSet();
}

// ------------------------------------------------------------------------------------------------
void RiderCommandSetUpgrade::crc(Xfer* xfer)
{
	UpgradeModule::crc(xfer);
}

// ------------------------------------------------------------------------------------------------
void RiderCommandSetUpgrade::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;

	xfer->xferVersion(&version, currentVersion);

	UpgradeModule::xfer(xfer);
}

// ------------------------------------------------------------------------------------------------
void RiderCommandSetUpgrade::loadPostProcess()
{
	UpgradeModule::loadPostProcess();
}
