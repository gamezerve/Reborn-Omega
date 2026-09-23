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

// FILE: RiderCommandSetUpgrade.h /////////////////////////////////////////////////////////////////////////////
// Author: Gamezerve, September 2026
// Desc: UpgradeModule that sets a new override string for Rider Command Set look ups
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Common/STLTypedefs.h"
#include "GameLogic/Module/UpgradeModule.h"

class Object;

//-----------------------------------------------------------------------------
struct RiderCommandSetMapping
{
	AsciiString m_sourceCommandSet;
	AsciiString m_targetCommandSet;
};

//-----------------------------------------------------------------------------
class RiderCommandSetUpgradeModuleData : public UpgradeModuleData
{
public:

	RiderCommandSetUpgradeModuleData();

	AsciiStringList m_applicableObjects;
	AsciiStringList m_applicableRiders;
	std::vector<RiderCommandSetMapping> m_commandSetMappings;

	static void buildFieldParse(MultiIniFieldParse& p);

	static void parseApplicableName(
		INI* ini,
		void* instance,
		void* store,
		const void* userData);

	static void parseCommandSetMapping(
		INI* ini,
		void* instance,
		void* store,
		const void* userData);
};

//-----------------------------------------------------------------------------
// Optional interface used by RiderChangeContain. This keeps RiderChangeContain
// independent from the concrete RiderCommandSetUpgrade implementation.
class RiderCommandSetUpgradeInterface
{
public:

	virtual Bool resolveCommandSet(
		const Object* object,
		const Object* rider,
		const AsciiString& sourceCommandSet,
		AsciiString& targetCommandSet) const = 0;
};

//-----------------------------------------------------------------------------
class RiderCommandSetUpgrade :
	public UpgradeModule,
	public RiderCommandSetUpgradeInterface
{

	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(
		RiderCommandSetUpgrade,
		"RiderCommandSetUpgrade")

		MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(
			RiderCommandSetUpgrade,
			RiderCommandSetUpgradeModuleData);

public:

	RiderCommandSetUpgrade(
		Thing* thing,
		const ModuleData* moduleData);

	virtual RiderCommandSetUpgradeInterface*
		getRiderCommandSetUpgradeInterface() override
	{
		return this;
	}

	virtual Bool resolveCommandSet(
		const Object* object,
		const Object* rider,
		const AsciiString& sourceCommandSet,
		AsciiString& targetCommandSet) const override;

protected:

	virtual void upgradeImplementation() override;
	virtual Bool isSubObjectsUpgrade() override { return false; }

};
