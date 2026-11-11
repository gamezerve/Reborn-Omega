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

#include "Common/ThingTemplate.h" // Reborn: Read the width of the rubble opening.
#include "Common/Xfer.h"
#include "GameLogic/GameLogic.h" // Reborn: Resolve ships tracked through the breach.
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
		// Reborn: The pull acts on the upstream side once the dam has collapsed.
		{ "RubblePullRange", INI::parseReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_rubblePullRange) },
		{ "RubblePullPeakAcceleration", INI::parseAccelerationReal, nullptr, offsetof(DamOutflowUpdateModuleData, m_rubblePullPeakAcceleration) },
		{ nullptr, nullptr, nullptr, 0 }
	};
	p.add(fields);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Initialize the current attached to the dam. */
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
/** Reborn: Push vessels downstream and pull them toward the opening after the dam collapses. */
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
	// Reborn: The same scan covers the spillway and the upstream pull area.
	const Real damHalfDepth = dam->getGeometryInfo().getMinorRadius();
	const Real passageHalfWidth = rubble ? dam->getTemplate()->getRubbleWaterPassageWidth() * 0.5f : 0.0f;
	const Real pullRange = passageHalfWidth > 0.0f && data->m_rubblePullPeakAcceleration > 0.0f &&
		data->m_rubblePullRange > damHalfDepth ? data->m_rubblePullRange : 0.0f;

	// Reborn: Track the drop beyond the current's scan and reuse each boat's normal death behavior.
	for (auto entry = m_fallingBoats.begin(); entry != m_fallingBoats.end(); )
	{
		Object* boat = TheGameLogic->findObjectByID(entry->first);
		if (pullRange <= 0.0f || !boat || !boat->getBodyModule() || boat->isEffectivelyDead() ||
			boat->getContainedBy() || boat->isDisabledByType(DISABLED_HELD))
		{
			entry = m_fallingBoats.erase(entry);
			continue;
		}

		const Coord3D& pos = *boat->getPosition();
		const Real dx = pos.x - origin.x;
		const Real dy = pos.y - origin.y;
		const Real downstream = dx * direction.x + dy * direction.y;
		const Real absLateral = WWMath::Fabsf(dx * along.x + dy * along.y);
		const Real clearance = boat->getGeometryInfo().getMinorRadius();
		if (downstream < -pullRange ||
			(downstream <= damHalfDepth + clearance && absLateral > passageHalfWidth - clearance))
		{
			entry = m_fallingBoats.erase(entry);
			continue;
		}

		if (downstream > 0.0f)
		{
			Real waterHeight;
			Real groundHeight;
			const Bool overWater = TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterHeight, &groundHeight);
			const Real surfaceHeight = overWater ? waterHeight : groundHeight;
			const Real minimumDrop = 10.0f;
			const Real landingTolerance = 5.0f; // Reborn: Account for a boat's hover height above water.
			if (surfaceHeight <= entry->second - minimumDrop && pos.z <= surfaceHeight + landingTolerance)
			{
				entry = m_fallingBoats.erase(entry);
				boat->kill();
				continue;
			}
		}
		++entry;
	}

	const Real scanOffset = (range - pullRange) * 0.5f;
	const Real halfRange = (range + pullRange) * 0.5f;
	const Coord3D scanCenter = { origin.x + direction.x * scanOffset, origin.y + direction.y * scanOffset, origin.z };
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
		const Real lateral = dx * along.x + dy * along.y;
		const Real absLateral = WWMath::Fabsf(lateral);
		if (absLateral >= halfWidth || downstream >= range || downstream < -pullRange)
			continue;

		const Real clearance = boat->getGeometryInfo().getMinorRadius();
		const Real opening = passageHalfWidth - clearance;
		const Real passageDepth = damHalfDepth + clearance;

		Real waterHeight;
		const Bool afloat = TheTerrainLogic->isUnderwater(pos.x, pos.y, &waterHeight) && pos.z <= waterHeight + 10.0f;
		const Coord3D& velocity = *physics->getVelocity();
		const Bool inFallingCurrent = pullRange > 0.0f && opening > 0.0f &&
			absLateral <= opening && WWMath::Fabsf(downstream) <= passageDepth + 10.0f &&
			velocity.x * direction.x + velocity.y * direction.y > 0.0f;

		if (downstream < 0.0f && pullRange > 0.0f && opening > 0.0f && (afloat || inFallingCurrent))
		{
			const Real strength = outflowStrength(-downstream, absLateral, damHalfDepth, pullRange, halfWidth);
			if (strength <= 0.0f)
				continue;

			// Reborn: Center ships before the lip, then aim through the opening.
			const Real targetDownstream = absLateral <= opening * 0.75f ?
				passageDepth + 10.0f : -(passageDepth + 10.0f);
			const Real forward = targetDownstream - downstream;
			const Real length = WWMath::Sqrtf(forward * forward + lateral * lateral);
			if (length > 0.0f)
			{
				const Real acceleration = data->m_rubblePullPeakAcceleration * strength / length;
				const Coord2D current = {
					(direction.x * forward - along.x * lateral) * acceleration,
					(direction.y * forward - along.y * lateral) * acceleration
				};
				physics->applyWaterCurrent(current);
				// Reborn: Only ships pulled into the opening from the upper water can be lost in the drop.
				if (afloat && absLateral <= opening && -downstream <= passageDepth + 10.0f)
					m_fallingBoats.try_emplace(boat->getID(), waterHeight);
			}
			continue;
		}

		// Reborn: Keep the falling stream moving through the short gap between the two water surfaces.
		Real acceleration = afloat ? peak * outflowStrength(downstream, absLateral, data->m_nearDistance, range, halfWidth) : 0.0f;
		if (inFallingCurrent && downstream >= 0.0f)
			acceleration = max(acceleration, data->m_rubblePullPeakAcceleration);
		if (acceleration > 0.0f)
		{
			const Coord2D current = { direction.x * acceleration, direction.y * acceleration };
			physics->applyWaterCurrent(current);
		}
	}
	return UPDATE_SLEEP_NONE;
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Transfer the upper water height of each ship being tracked through the breach. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::xferFallingBoats(Xfer* xfer)
{
	UnsignedInt count = static_cast<UnsignedInt>(m_fallingBoats.size());
	xfer->xferUnsignedInt(&count);
	if (xfer->getXferMode() == XFER_LOAD)
	{
		m_fallingBoats.clear();
		for (UnsignedInt index = 0; index < count; ++index)
		{
			ObjectID id = INVALID_ID;
			Real upperWaterHeight = 0.0f;
			xfer->xferObjectID(&id);
			xfer->xferReal(&upperWaterHeight);
			m_fallingBoats.emplace(id, upperWaterHeight);
		}
		return;
	}

	for (auto [id, upperWaterHeight] : m_fallingBoats)
	{
		ObjectID boatId = id;
		xfer->xferObjectID(&boatId);
		xfer->xferReal(&upperWaterHeight);
	}
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Include the pending falls in the dam's checksum. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::crc(Xfer* xfer)
{
	UpdateModule::crc(xfer);
	xferFallingBoats(xfer);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Save the scheduler and any ships already entering or falling through the breach. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::xfer(Xfer* xfer)
{
	const XferVersion currentVersion = 2;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);
	UpdateModule::xfer(xfer);
	if (version >= 2)
		xferFallingBoats(xfer);
}

//-------------------------------------------------------------------------------------------------
/** Reborn: Recompute the current from the dam and terrain after loading. */
//-------------------------------------------------------------------------------------------------
void DamOutflowUpdate::loadPostProcess()
{
	UpdateModule::loadPostProcess();
}
