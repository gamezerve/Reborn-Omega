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

#pragma once

#include "GameLogic/Module/UpdateModule.h"

// Reborn: Configure the downstream current for the intact dam and its rubble.
class DamOutflowUpdateModuleData : public ModuleData
{
public:
	Real m_nearDistance = 0.0f;
	Real m_range = 0.0f;
	Real m_width = 0.0f;
	Real m_peakAcceleration = 0.0f;
	Real m_rubbleRange = 0.0f;
	Real m_rubbleWidth = 0.0f;
	Real m_rubblePeakAcceleration = 0.0f;

	static void buildFieldParse(MultiIniFieldParse& p);
};

// Reborn: Push nearby vessels from the dam's spillway before their physics update.
class DamOutflowUpdate : public UpdateModule
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(DamOutflowUpdate, "DamOutflowUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(DamOutflowUpdate, DamOutflowUpdateModuleData);

public:
	DamOutflowUpdate(Thing* thing, const ModuleData* moduleData);

	virtual UpdateSleepTime update() override;
	// Reborn: Continue while the dam is rubble.
	virtual DisabledMaskType getDisabledTypesToProcess() const override { return DISABLEDMASK_ALL; }

protected:
	// Reborn: Queue the current before physics moves the vessels.
	virtual SleepyUpdatePhase getUpdatePhase() const override { return PHASE_INITIAL; }
};
