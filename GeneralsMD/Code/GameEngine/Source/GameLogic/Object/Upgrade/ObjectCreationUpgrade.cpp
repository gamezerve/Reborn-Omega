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

// FILE: ObjectCreationUpgrade.cpp /////////////////////////////////////////////////////////////////////////////
// Author: Matthew D. Campbell, April 2002
// Desc:	 upgrades that spawn OCLs
///////////////////////////////////////////////////////////////////////////////////////////////////

// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////
#include "PreRTS.h"	// This must go first in EVERY cpp file in the GameEngine

#include "Common/ModelState.h"
#include "Common/Player.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameLogic/Module/ObjectCreationUpgrade.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectCreationList.h"

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgradeModuleData::ObjectCreationUpgradeModuleData()
{

	m_ocl = nullptr;
	m_tooltipTriggerUpgradeName.clear();

}

static void parseObjectCreationUpgradeTriggeredBy(INI* ini, void* instance, void* store, const void* /*userData*/)
{
	ObjectCreationUpgradeModuleData* data = static_cast<ObjectCreationUpgradeModuleData*>(store);

	INI::parseAsciiStringVector(
		ini,
		instance,
		&data->m_upgradeMuxData.m_activationUpgradeNames,
		nullptr);

	if (data->m_tooltipTriggerUpgradeName.isEmpty() && !data->m_upgradeMuxData.m_activationUpgradeNames.empty())
	{
		data->m_tooltipTriggerUpgradeName = data->m_upgradeMuxData.m_activationUpgradeNames[0];
	}
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
/* static */ void ObjectCreationUpgradeModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "TriggeredBy", parseObjectCreationUpgradeTriggeredBy, nullptr, 0 },
		{ "ConflictsWith", INI::parseAsciiStringVector, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_upgradeMuxData.m_conflictingUpgradeNames) },
		{ "RemovesUpgrades", INI::parseAsciiStringVector, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_upgradeMuxData.m_removalUpgradeNames) },
		{ "FXListUpgrade", INI::parseFXList, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_upgradeMuxData.m_fxListUpgrade) },
		{ "RequiresAllTriggers", INI::parseBool, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_upgradeMuxData.m_requiresAllTriggers) },
		{ "UpgradeObject", INI::parseObjectCreationList, nullptr, offsetof(ObjectCreationUpgradeModuleData, m_ocl) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(dataFieldParse);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::ObjectCreationUpgrade( Thing *thing, const ModuleData* moduleData ) :
							UpgradeModule( thing, moduleData )
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ObjectCreationUpgrade::~ObjectCreationUpgrade()
{

}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::onDelete()
{
}

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::upgradeImplementation()
{
	// spawn everything in the OCL
	if (getObjectCreationUpgradeModuleData() && getObjectCreationUpgradeModuleData()->m_ocl)
	{
		ObjectCreationList::create((getObjectCreationUpgradeModuleData()->m_ocl), getObject(), nullptr);
	}
}

// ------------------------------------------------------------------------------------------------
/** CRC */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::crc( Xfer *xfer )
{

	// extend base class
	UpgradeModule::crc( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Xfer method
	* Version Info:
	* 1: Initial version */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::xfer( Xfer *xfer )
{

	// version
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion( &version, currentVersion );

	// extend base class
	UpgradeModule::xfer( xfer );

}

// ------------------------------------------------------------------------------------------------
/** Load post process */
// ------------------------------------------------------------------------------------------------
void ObjectCreationUpgrade::loadPostProcess()
{

	// extend base class
	UpgradeModule::loadPostProcess();

}
