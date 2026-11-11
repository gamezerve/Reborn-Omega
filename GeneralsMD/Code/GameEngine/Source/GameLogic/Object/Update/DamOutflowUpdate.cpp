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

#include "PreRTS.h"

#include "Common/Xfer.h"
#include "GameLogic/Module/BodyModule.h"
#include "GameLogic/Module/DamOutflowUpdate.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/Object.h"
#include "GameLogic/ObjectIter.h"
#include "GameLogic/PartitionManager.h"
#include "GameLogic/TerrainLogic.h"

//-------------------------------------------------------------------------------------------------
/** Reborn: Convert the configured accelerations from world units per second squared. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	ModuleData::buildFieldParse(p);
	static const FieldParse fields[] =
	{
		{ "NearDistance", INI::parseReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_nearDistance) },
		{ "Range", INI::parseReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_range) },
		{ "Width", INI::parseReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_width) },
		{ "PeakAcceleration", INI::parseAccelerationReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_peakAcceleration) },
		{ "RubbleRange", INI::parseReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_rubbleRange) },
		{ "RubbleWidth", INI::parseReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_rubbleWidth) },
		{ "RubblePeakAcceleration", INI::parseAccelerationReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_rubblePeakAcceleration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Initialize a stateless current attached to the dam. */
//-------------------------------------------------------------------------------------------------
DamOutflowUpdate::DamOutflowUpdate(Thing* thing, const ModuleData* moduleData)
	: UpdateModule(thing, moduleData)
{
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Release the dam's current module. */
//-------------------------------------------------------------------------------------------------
DamOutflowUpdate::~DamOutflowUpdate() = default;

//-------------------------------------------------------------------------------------------------
/** Reborn: Increase current exponentially toward the dam and fade it at the side edges. */
//-------------------------------------------------------------------------------------------------
static Real outflowStrength(Real downstream, Real lateral, Real nearDistance, Real range, Real halfWidth)
{
	if (downstream <= 0.0f || downstream >= range || lateral >= halfWidth)
		return 0.0f;

	const Real closeness = min(1.0f, (range - downstream) / (range - nearDistance));
	const Real exponent = 3.0f;
	const Real longitudinal = (WWMath::Expf(exponent * closeness) - 1.0f) /
		(WWMath::Expf(exponent) - 1.0f);
	const Real sideFadeWidth = min(20.0f, halfWidth);
	const Real edge = min(1.0f, (halfWidth - lateral) / sideFadeWidth);
	return longitudinal * edge * edge * (3.0f - 2.0f * edge);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Apply the dam-local current only to vessels in its downstream water. */
//-------------------------------------------------------------------------------------------------
UpdateSleepTime DamOutflowUpdate::update()
{
	const Object* dam = getObject();
	const DamOutflowUpdateModuleData* data = getDamOutflowUpdateModuleData();
	const Bool rubble = dam->getBodyModule()->getDamageState() == BODY_RUBBLE;
	const Real range = rubble ? data->m_rubbleRange : data->m_range;
	const Real halfWidth = 0.5f * (rubble ? data->m_rubbleWidth : data->m_width);
	const Real peak = rubble ? data->m_rubblePeakAcceleration : data->m_peakAcceleration;
	if (data->m_nearDistance < 0.0f || range <= data->m_nearDistance || halfWidth <= 0.0f || peak <= 0.0f)
		return UPDATE_SLEEP_NONE;

	const Coord3D& origin = *dam->getPosition();
	const Coord3D& along = *dam->getUnitDirectionVector2D();
	const Coord2D direction = { along.y, -along.x };
	const Real halfRange = range * 0.5f;
	const Coord3D scanCenter = { origin.x + direction.x * halfRange, origin.y + direction.y * halfRange, origin.z };
	const Real scanRadius = WWMath::Sqrtf(halfRange * halfRange + halfWidth * halfWidth);

	PartitionFilterAcceptByKindOf boats(MAKE_KINDOF_MASK(KINDOF_BOAT), MAKE_KINDOF_MASK(KINDOF_IMMOBILE));
	PartitionFilter* filters[] = { &boats, nullptr };
	ObjectIterator* iter = ThePartitionManager->iterateObjectsInRange(&scanCenter, scanRadius, FROM_CENTER_2D, filters);
	MemoryPoolObjectHolder hold(iter);
	for (Object* boat = iter->first(); boat; boat = iter->next())
	{
		PhysicsBehavior* physics = boat->getPhysics();
		if (!physics || boat->isEffectivelyDead() || boat->getContainedBy() || boat->isDisabledByType(DISABLED_HELD))
			continue;

		const Coord3D& pos = *boat->getPosition();
		const Real dx = pos.x - origin.x;
		const Real dy = pos.y - origin.y;
		const Real downstream = dx * direction.x + dy * direction.y;
		const Real lateral = WWMath::Fabsf(dx * along.x + dy * along.y);
		const Real strength = outflowStrength(downstream, lateral, data->m_nearDistance, range, halfWidth);
		if (strength <= 0.0f)
			continue;

		Real waterHeight;
		if (!TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterHeight) || pos.z > waterHeight + 10.0f)
			continue;

		// Reborn: Use acceleration directly, equivalent to scaling force by the vessel mass.
		const Real acceleration = peak * strength;
		const Coord2D current = { direction.x * acceleration, direction.y * acceleration };
		physics->applyWaterCurrent(current);
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: The current has no state beyond the update scheduler. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Save the scheduler state for the dam's current. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Recompute the current from the dam and terrain after loading. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
