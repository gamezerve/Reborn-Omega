/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

///////////////////////////////////////////////////////////////////////////////////////
// FILE: ComancheTransportAIUpdate.cpp ////////////////////////////////////////////////
// Author: Gamezerve, October 2026
// Description: Implements JetAIUpdate-based transport and two-rope rappel behavior for Comanche aircraft.
///////////////////////////////////////////////////////////////////////////////////////

#include "PreRTS.h"

#include "Common/ActionManager.h"
#include "Common/DrawModule.h"
#include "Common/GlobalData.h"
#include "Common/RandomValue.h"
#include "Common/ThingFactory.h"
#include "Common/ThingTemplate.h"
#include "Common/Xfer.h"
#include "GameClient/Drawable.h"
#include "GameClient/GameClient.h"
#include "GameLogic/Locomotor.h"
#include "GameLogic/Module/ComancheTransportAIUpdate.h"
#include "GameLogic/Module/ContainModule.h"
#include "GameLogic/Module/PhysicsUpdate.h"
#include "GameLogic/PartitionManager.h"

//-------------------------------------------------------------------------------------------------
static void initRopeParms(Drawable* rope, Real length, Real width, const RGBColor& color, Real wobbleLen, Real wobbleAmp, Real wobbleRate)
{
	RopeDrawInterface* ropeDraw = nullptr;
	for (DrawModule** draw = rope->getDrawModules(); *draw; ++draw)
	{
		if ((ropeDraw = (*draw)->getRopeDrawInterface()) != nullptr)
		{
			ropeDraw->initRopeParms(length, width, color, wobbleLen, wobbleAmp, wobbleRate);
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void setRopeCurLen(Drawable* rope, Real length)
{
	RopeDrawInterface* ropeDraw = nullptr;
	for (DrawModule** draw = rope->getDrawModules(); *draw; ++draw)
	{
		if ((ropeDraw = (*draw)->getRopeDrawInterface()) != nullptr)
		{
			ropeDraw->setRopeCurLen(length);
		}
	}
}

//-------------------------------------------------------------------------------------------------
static void setRopeSpeed(Drawable* rope, Real curSpeed, Real maxSpeed, Real accel)
{
	RopeDrawInterface* ropeDraw = nullptr;
	for (DrawModule** draw = rope->getDrawModules(); *draw; ++draw)
	{
		if ((ropeDraw = (*draw)->getRopeDrawInterface()) != nullptr)
		{
			ropeDraw->setRopeSpeed(curSpeed, maxSpeed, accel);
		}
	}
}

//-------------------------------------------------------------------------------------------------
ComancheTransportAIUpdateModuleData::ComancheTransportAIUpdateModuleData()
{
	m_ropeName = "GenericRope";
	m_rappelSpeed = fabs(TheGlobalData->m_gravity) * (Real)LOGICFRAMES_PER_SECOND * 0.5f;
	m_ropeDropSpeed = 1e10f;
	m_ropeWidth = 0.5f;
	m_ropeFinalHeight = 10.0f;
	m_ropeWobbleLen = 10.0f;
	m_ropeWobbleAmp = 0.25f;
	m_ropeWobbleRate = 180.0f;
	m_ropeColor.red = 0.0f;
	m_ropeColor.green = 0.0f;
	m_ropeColor.blue = 0.0f;
	m_perRopeDelayMin = 900;
	m_perRopeDelayMax = 1500;
	m_minDropHeight = 40.0f;
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdateModuleData::buildFieldParse(MultiIniFieldParse& p)
{
	JetAIUpdateModuleData::buildFieldParse(p);

	static const FieldParse dataFieldParse[] =
	{
		{ "RappelSpeed", INI::parseVelocityReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_rappelSpeed) },
		{ "RopeDropSpeed", INI::parseVelocityReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeDropSpeed) },
		{ "RopeName", INI::parseAsciiString, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeName) },
		{ "RopeFinalHeight", INI::parseReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeFinalHeight) },
		{ "RopeWidth", INI::parseReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeWidth) },
		{ "RopeWobbleLen", INI::parseReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeWobbleLen) },
		{ "RopeWobbleAmplitude", INI::parseReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeWobbleAmp) },
		{ "RopeWobbleRate", INI::parseAngularVelocityReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeWobbleRate) },
		{ "RopeColor", INI::parseRGBColor, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_ropeColor) },
		{ "PerRopeDelayMin", INI::parseDurationUnsignedInt, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_perRopeDelayMin) },
		{ "PerRopeDelayMax", INI::parseDurationUnsignedInt, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_perRopeDelayMax) },
		{ "MinDropHeight", INI::parseReal, nullptr, offsetof(ComancheTransportAIUpdateModuleData, m_minDropHeight) },
		{ nullptr, nullptr, nullptr, 0 }
	};

	p.add(dataFieldParse);
}

//-------------------------------------------------------------------------------------------------
ComancheTransportAIUpdate::ComancheTransportAIUpdate(Thing* thing, const ModuleData* moduleData)
	: JetAIUpdate(thing, moduleData)
{
	m_dropState = DROP_NONE;
	m_dropPosition.zero();
	m_dropTargetID = INVALID_ID;
	m_requestedExitID = INVALID_ID;
	m_dropAllPassengers = FALSE;
	m_ropeCount = 0;
	m_oldPreferredHeight = 0.0f;
	m_preferredHeightAdjusted = FALSE;

	for (Int i = 0; i < 2; ++i)
	{
		m_ropes[i].ropeDrawable = nullptr;
		m_ropes[i].ropeID = INVALID_DRAWABLE_ID;
		m_ropes[i].ropeSpeed = 0.0f;
		m_ropes[i].ropeLen = 0.0f;
		m_ropes[i].ropeLenMax = 0.0f;
		m_ropes[i].nextDropTime = 0;
		m_ropes[i].rappellerID = INVALID_ID;
	}
}

//-------------------------------------------------------------------------------------------------
ComancheTransportAIUpdate::~ComancheTransportAIUpdate()
{
}

//-------------------------------------------------------------------------------------------------
Object* ComancheTransportAIUpdate::getPotentialRappeller() const
{
	ContainModuleInterface* contain = getObject()->getContain();
	const ContainedItemsList* items = contain ? contain->getContainedItemsList() : nullptr;
	if (!items)
		return nullptr;

	if (m_requestedExitID != INVALID_ID)
	{
		Object* requested = TheGameLogic->findObjectByID(m_requestedExitID);
		if (requested && contain->isContained(requested) && requested->isKindOf(KINDOF_CAN_RAPPEL))
			return requested;

		m_requestedExitID = INVALID_ID;
	}

	if (!m_dropAllPassengers)
		return nullptr;

	for (ContainedItemsList::const_iterator it = items->begin(); it != items->end(); ++it)
	{
		Object* passenger = *it;
		if (passenger && passenger->isKindOf(KINDOF_CAN_RAPPEL))
			return passenger;
	}

	return nullptr;
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::cleanupFinishedRappellers()
{
	for (Int i = 0; i < m_ropeCount; ++i)
	{
		if (m_ropes[i].rappellerID == INVALID_ID)
			continue;

		Object* rappeller = TheGameLogic->findObjectByID(m_ropes[i].rappellerID);
		if (rappeller == nullptr || rappeller->isEffectivelyDead() || !rappeller->isAboveTerrain() || rappeller->isContained())
		{
			m_ropes[i].rappellerID = INVALID_ID;
		}
	}
}

//-------------------------------------------------------------------------------------------------
Bool ComancheTransportAIUpdate::createRopes()
{
	Object* obj = getObject();
	Drawable* draw = obj->getDrawable();
	if (!draw)
		return FALSE;

	const Int MAX_ROPES = 2;
	Coord3D ropePos[MAX_ROPES];
	Matrix3D dropMtx[MAX_ROPES];

	const Int ropeCount = draw->getPristineBonePositions("RopeStart", 1, ropePos, nullptr, MAX_ROPES);
	const Int dropCount = draw->getPristineBonePositions("RopeEnd", 1, nullptr, dropMtx, MAX_ROPES);
	m_ropeCount = min(MAX_ROPES, min(ropeCount, dropCount));

	if (m_ropeCount <= 0)
	{
		DEBUG_LOG(("ComancheTransportAIUpdate: no RopeStart/RopeEnd bones found on %s\n",
			obj->getTemplate()->getName().str()));
		return FALSE;
	}

	const ComancheTransportAIUpdateModuleData* data = getComancheTransportAIUpdateModuleData();
	const ThingTemplate* ropeTemplate = TheThingFactory->findTemplate(data->m_ropeName);
	const UnsignedInt now = TheGameLogic->getFrame();

	for (Int i = 0; i < m_ropeCount; ++i)
	{
		RopeInfo& rope = m_ropes[i];

		obj->convertBonePosToWorldPos(&ropePos[i], nullptr, &ropePos[i], nullptr);

		rope.ropeDrawable = ropeTemplate ? TheThingFactory->newDrawable(ropeTemplate) : nullptr;
		rope.ropeID = INVALID_DRAWABLE_ID;
		rope.ropeSpeed = 0.0f;
		rope.ropeLen = 1.0f;
		rope.rappellerID = INVALID_ID;
		rope.nextDropTime = now + GameLogicRandomValue(data->m_perRopeDelayMin, data->m_perRopeDelayMax) - data->m_perRopeDelayMin;

		const Bool onlyHealthyBridges = TRUE;
		const PathfindLayerEnum layerAtDest = TheTerrainLogic->getHighestLayerForDestination(&ropePos[i], onlyHealthyBridges);
		rope.ropeLenMax = ropePos[i].z - TheTerrainLogic->getLayerHeight(ropePos[i].x, ropePos[i].y, layerAtDest) - data->m_ropeFinalHeight;
		if (rope.ropeLenMax < 1.0f)
			rope.ropeLenMax = 1.0f;

		if (rope.ropeDrawable)
		{
			rope.ropeDrawable->setPosition(&ropePos[i]);
			initRopeParms(
				rope.ropeDrawable,
				rope.ropeLenMax,
				data->m_ropeWidth,
				data->m_ropeColor,
				data->m_ropeWobbleLen,
				data->m_ropeWobbleAmp,
				data->m_ropeWobbleRate);
		}
	}

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
Bool ComancheTransportAIUpdate::dropNextPassenger(Int ropeIndex)
{
	if (ropeIndex < 0 || ropeIndex >= m_ropeCount)
		return FALSE;

	Object* passenger = getPotentialRappeller();
	if (!passenger)
		return FALSE;

	Object* obj = getObject();
	Drawable* draw = obj->getDrawable();
	if (!draw)
		return FALSE;

	Matrix3D dropMtx[2];
	const Int dropCount = draw->getPristineBonePositions("RopeEnd", 1, nullptr, dropMtx, 2);
	if (ropeIndex >= dropCount)
		return FALSE;

	Matrix3D worldDropMtx;
	obj->convertBonePosToWorldPos(nullptr, &dropMtx[ropeIndex], nullptr, &worldDropMtx);

	ExitInterface* exitInterface = obj->getObjectExitInterface();
	if (!exitInterface)
		return FALSE;

	exitInterface->exitObjectViaDoor(passenger, DOOR_1);
	passenger->setTransformMatrix(&worldDropMtx);

	AIUpdateInterface* passengerAI = passenger->getAIUpdateInterface();
	if (!passengerAI)
		return FALSE;

	const ComancheTransportAIUpdateModuleData* data = getComancheTransportAIUpdateModuleData();
	Object* target = TheGameLogic->findObjectByID(m_dropTargetID);
	passengerAI->setDesiredSpeed(data->m_rappelSpeed);
	passengerAI->aiRappelInto(target, m_dropPosition, CMD_FROM_AI);

	m_ropes[ropeIndex].rappellerID = passenger->getID();
	m_ropes[ropeIndex].nextDropTime =
		TheGameLogic->getFrame() + GameLogicRandomValue(data->m_perRopeDelayMin, data->m_perRopeDelayMax);

	if (passenger->getID() == m_requestedExitID)
		m_requestedExitID = INVALID_ID;

	return TRUE;
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::beginRappel(Object* target, const Coord3D& pos, Bool dropAllPassengers)
{
	if (m_dropState == DROP_RAPPELLING)
		cancelRappel();

	m_dropTargetID = target ? target->getID() : INVALID_ID;
	m_dropPosition = pos;
	m_dropAllPassengers = dropAllPassengers;

	AIUpdateInterface::privateIdle(CMD_FROM_AI);
	setLocomotorGoalNone();
	getObject()->getPhysics()->scrubVelocity2D(0);

	if (!createRopes())
	{
		cancelRappel();
		return;
	}

	m_dropState = DROP_RAPPELLING;
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::finishRappel()
{
	const ComancheTransportAIUpdateModuleData* data = getComancheTransportAIUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();

	for (Int i = 0; i < m_ropeCount; ++i)
	{
		if (m_ropes[i].ropeDrawable)
		{
			const UnsignedInt ROPE_EXPIRATION_TIME = LOGICFRAMES_PER_SECOND * 5;
			const Real initialSpeed = TheGlobalData->m_gravity * 30.0f;
			setRopeSpeed(m_ropes[i].ropeDrawable, initialSpeed, data->m_ropeDropSpeed, TheGlobalData->m_gravity);
			m_ropes[i].ropeDrawable->setExpirationDate(now + ROPE_EXPIRATION_TIME);
			m_ropes[i].ropeDrawable = nullptr;
		}

		m_ropes[i].ropeID = INVALID_DRAWABLE_ID;
		m_ropes[i].rappellerID = INVALID_ID;
	}

	m_ropeCount = 0;
	m_dropState = DROP_NONE;
	m_dropTargetID = INVALID_ID;
	m_dropAllPassengers = FALSE;

	if (m_preferredHeightAdjusted)
	{
		Locomotor* loco = getCurLocomotor();
		if (loco)
		{
			loco->setPreferredHeight(m_oldPreferredHeight);
			loco->setUltraAccurate(FALSE);
		}
		m_preferredHeightAdjusted = FALSE;
	}

	AIUpdateInterface::privateIdle(CMD_FROM_AI);
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::cancelRappel()
{
	const ComancheTransportAIUpdateModuleData* data = getComancheTransportAIUpdateModuleData();
	const UnsignedInt now = TheGameLogic->getFrame();

	for (Int i = 0; i < m_ropeCount; ++i)
	{
		if (m_ropes[i].ropeDrawable)
		{
			const UnsignedInt ROPE_EXPIRATION_TIME = LOGICFRAMES_PER_SECOND * 5;
			const Real initialSpeed = TheGlobalData->m_gravity * 30.0f;
			setRopeSpeed(m_ropes[i].ropeDrawable, initialSpeed, data->m_ropeDropSpeed, TheGlobalData->m_gravity);
			m_ropes[i].ropeDrawable->setExpirationDate(now + ROPE_EXPIRATION_TIME);
			m_ropes[i].ropeDrawable = nullptr;
		}

		m_ropes[i].ropeID = INVALID_DRAWABLE_ID;
		m_ropes[i].rappellerID = INVALID_ID;
	}

	m_ropeCount = 0;
	m_dropState = DROP_NONE;
	m_dropTargetID = INVALID_ID;
	m_dropAllPassengers = FALSE;
	m_requestedExitID = INVALID_ID;

	if (m_preferredHeightAdjusted)
	{
		Locomotor* loco = getCurLocomotor();
		if (loco)
		{
			loco->setPreferredHeight(m_oldPreferredHeight);
			loco->setUltraAccurate(FALSE);
		}
		m_preferredHeightAdjusted = FALSE;
	}
}

//-------------------------------------------------------------------------------------------------
AIFreeToExitType ComancheTransportAIUpdate::getAiFreeToExit(const Object* exiter) const
{
	if (exiter && getObject()->getContain() && getObject()->getContain()->isContained(exiter))
	{
		m_requestedExitID = exiter->getID();
		return WAIT_TO_EXIT;
	}

	return FREE_TO_EXIT;
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::privateEvacuate(Int exposeStealthUnits, CommandSourceType cmdSource)
{
	if (getObject()->isDisabledByType(DISABLED_SUBDUED))
		return;

	ContainModuleInterface* contain = getObject()->getContain();
	if (!contain || contain->getContainCount() == 0)
		return;

	if (exposeStealthUnits)
		contain->markAllPassengersDetected();

	m_requestedExitID = INVALID_ID;
	beginRappel(nullptr, *getObject()->getPosition(), TRUE);
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::privateCombatDrop(Object* target, const Coord3D& pos, CommandSourceType cmdSource)
{
	if (target != nullptr && cmdSource == CMD_FROM_PLAYER &&
		TheActionManager->canEnterObject(getObject(), target, cmdSource, COMBATDROP_INTO) == FALSE)
	{
		return;
	}

	ContainModuleInterface* contain = getObject()->getContain();
	if (!contain || contain->getContainCount() == 0)
		return;

	Coord3D localPos = pos;
	if (target == nullptr)
	{
		Coord3D tmp;
		FindPositionOptions options;
		options.maxRadius = getObject()->getGeometryInfo().getBoundingCircleRadius() * 100.0f;
		if (ThePartitionManager->findPositionAround(&localPos, &options, &tmp))
			localPos = tmp;
	}
	else
	{
		localPos = *target->getPosition();

		Locomotor* loco = getCurLocomotor();
		if (loco)
		{
			const ComancheTransportAIUpdateModuleData* data = getComancheTransportAIUpdateModuleData();
			m_oldPreferredHeight = loco->getPreferredHeight();
			Real preferredHeight = target->getGeometryInfo().getMaxHeightAbovePosition() + data->m_minDropHeight;
			if (preferredHeight < m_oldPreferredHeight)
				preferredHeight = m_oldPreferredHeight;

			loco->setPreferredHeight(preferredHeight);
			loco->setUltraAccurate(TRUE);
			m_preferredHeightAdjusted = TRUE;
		}
	}

	m_dropTargetID = target ? target->getID() : INVALID_ID;
	m_dropPosition = localPos;
	m_dropAllPassengers = TRUE;
	m_requestedExitID = INVALID_ID;
	m_dropState = DROP_MOVING_TO_TARGET;

	AIUpdateInterface::privateMoveToPosition(&m_dropPosition, cmdSource);
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::aiDoCommand(const AICommandParms* parms)
{
	if (m_dropState != DROP_NONE && parms->m_cmdSource != CMD_FROM_AI)
	{
		cancelRappel();
	}

	JetAIUpdate::aiDoCommand(parms);
}

//-------------------------------------------------------------------------------------------------
UpdateSleepTime ComancheTransportAIUpdate::update()
{
	UpdateSleepTime result = JetAIUpdate::update();

	ContainModuleInterface* contain = getObject()->getContain();
	if (m_dropState == DROP_NONE &&
		m_requestedExitID != INVALID_ID &&
		contain &&
		contain->hasObjectsWantingToEnterOrExit())
	{
		beginRappel(nullptr, *getObject()->getPosition(), FALSE);
	}

	if (m_dropState == DROP_MOVING_TO_TARGET)
	{
		const Coord3D* pos = getObject()->getPosition();
		const Real dx = pos->x - m_dropPosition.x;
		const Real dy = pos->y - m_dropPosition.y;
		const Real horizontalThreshold = 8.0f;

		Bool altitudeReady = TRUE;
		Locomotor* loco = getCurLocomotor();
		if (loco)
		{
			const Real desiredZ =
				TheTerrainLogic->getGroundHeight(m_dropPosition.x, m_dropPosition.y) + loco->getPreferredHeight();
			altitudeReady = fabs(pos->z - desiredZ) <= 3.0f;
		}

		if (dx * dx + dy * dy <= horizontalThreshold * horizontalThreshold && altitudeReady)
		{
			Object* target = TheGameLogic->findObjectByID(m_dropTargetID);
			beginRappel(target, m_dropPosition, TRUE);
		}
	}
	else if (m_dropState == DROP_RAPPELLING)
	{
		setLocomotorGoalNone();
		getObject()->getPhysics()->scrubVelocity2D(0);

		cleanupFinishedRappellers();

		const ComancheTransportAIUpdateModuleData* data = getComancheTransportAIUpdateModuleData();
		const UnsignedInt now = TheGameLogic->getFrame();

		for (Int i = 0; i < m_ropeCount; ++i)
		{
			RopeInfo& rope = m_ropes[i];

			if (rope.ropeLen < rope.ropeLenMax)
			{
				rope.ropeSpeed += fabs(TheGlobalData->m_gravity);
				if (rope.ropeSpeed > data->m_ropeDropSpeed)
					rope.ropeSpeed = data->m_ropeDropSpeed;

				rope.ropeLen += rope.ropeSpeed;
				if (rope.ropeLen > rope.ropeLenMax)
					rope.ropeLen = rope.ropeLenMax;

				if (rope.ropeDrawable)
					setRopeCurLen(rope.ropeDrawable, rope.ropeLen);

				continue;
			}

			if (rope.rappellerID == INVALID_ID && now >= rope.nextDropTime)
				dropNextPassenger(i);
		}

		Bool anyActiveRappeller = FALSE;
		for (Int i = 0; i < m_ropeCount; ++i)
		{
			if (m_ropes[i].rappellerID != INVALID_ID)
			{
				anyActiveRappeller = TRUE;
				break;
			}
		}

		if (!anyActiveRappeller && getPotentialRappeller() == nullptr)
			finishRappel();
	}

	return result;
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::crc(Xfer* xfer)
{
	JetAIUpdate::crc(xfer);
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::xfer(Xfer* xfer)
{
	XferVersion currentVersion = 1;
	XferVersion version = currentVersion;
	xfer->xferVersion(&version, currentVersion);

	JetAIUpdate::xfer(xfer);

	Int dropState = static_cast<Int>(m_dropState);
	xfer->xferInt(&dropState);
	if (xfer->getXferMode() == XFER_LOAD)
		m_dropState = static_cast<DropState>(dropState);

	xfer->xferCoord3D(&m_dropPosition);
	xfer->xferObjectID(&m_dropTargetID);
	xfer->xferObjectID(&m_requestedExitID);
	xfer->xferBool(&m_dropAllPassengers);
	xfer->xferInt(&m_ropeCount);
	xfer->xferReal(&m_oldPreferredHeight);
	xfer->xferBool(&m_preferredHeightAdjusted);

	for (Int i = 0; i < 2; ++i)
	{
		RopeInfo& rope = m_ropes[i];
		if (xfer->getXferMode() == XFER_SAVE)
			rope.ropeID = rope.ropeDrawable ? rope.ropeDrawable->getID() : INVALID_DRAWABLE_ID;

		xfer->xferDrawableID(&rope.ropeID);
		xfer->xferReal(&rope.ropeSpeed);
		xfer->xferReal(&rope.ropeLen);
		xfer->xferReal(&rope.ropeLenMax);
		xfer->xferUnsignedInt(&rope.nextDropTime);
		xfer->xferObjectID(&rope.rappellerID);

		if (xfer->getXferMode() == XFER_LOAD)
			rope.ropeDrawable = nullptr;
	}
}

//-------------------------------------------------------------------------------------------------
void ComancheTransportAIUpdate::loadPostProcess()
{
	JetAIUpdate::loadPostProcess();

	for (Int i = 0; i < 2; ++i)
	{
		if (m_ropes[i].ropeID != INVALID_DRAWABLE_ID)
			m_ropes[i].ropeDrawable = TheGameClient->findDrawableByID(m_ropes[i].ropeID);

		m_ropes[i].ropeID = INVALID_DRAWABLE_ID;
	}
}
