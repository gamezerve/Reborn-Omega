/*
** Command & Conquer Generals Zero Hour(tm)
** Copyright 2025 Electronic Arts Inc.
**
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
*/

#pragma once

#include "GameClient/Color.h"
#include "GameLogic/Module/JetAIUpdate.h"

class Drawable;

//-------------------------------------------------------------------------------------------------
class ComancheTransportAIUpdateModuleData : public JetAIUpdateModuleData
{
public:
	AsciiString m_ropeName;
	Real m_rappelSpeed;
	Real m_ropeDropSpeed;
	Real m_ropeWidth;
	Real m_ropeFinalHeight;
	Real m_ropeWobbleLen;
	Real m_ropeWobbleAmp;
	Real m_ropeWobbleRate;
	RGBColor m_ropeColor;
	UnsignedInt m_perRopeDelayMin;
	UnsignedInt m_perRopeDelayMax;
	Real m_minDropHeight;

	ComancheTransportAIUpdateModuleData();
	static void buildFieldParse(MultiIniFieldParse& p);
};

//-------------------------------------------------------------------------------------------------
class ComancheTransportAIUpdate : public JetAIUpdate
{
	MEMORY_POOL_GLUE_WITH_USERLOOKUP_CREATE(ComancheTransportAIUpdate, "ComancheTransportAIUpdate")
	MAKE_STANDARD_MODULE_MACRO_WITH_MODULE_DATA(ComancheTransportAIUpdate, ComancheTransportAIUpdateModuleData)

public:
	ComancheTransportAIUpdate(Thing* thing, const ModuleData* moduleData);
	virtual AIFreeToExitType getAiFreeToExit(const Object* exiter) const override;
	virtual void aiDoCommand(const AICommandParms* parms) override;

protected:
	virtual UpdateSleepTime update() override;
	virtual void privateCombatDrop(Object* target, const Coord3D& pos, CommandSourceType cmdSource) override;
	virtual void privateEvacuate(Int exposeStealthUnits, CommandSourceType cmdSource) override;

private:
	enum DropState CPP_11(: Int)
	{
		DROP_NONE = 0,
		DROP_MOVING_TO_TARGET,
		DROP_RAPPELLING
	};

	struct RopeInfo
	{
		Drawable* ropeDrawable;
		DrawableID ropeID;
		Real ropeSpeed;
		Real ropeLen;
		Real ropeLenMax;
		UnsignedInt nextDropTime;
		ObjectID rappellerID;
	};

	const ComancheTransportAIUpdateModuleData* getComancheTransportAIUpdateModuleData() const
	{
		return static_cast<const ComancheTransportAIUpdateModuleData*>(getModuleData());
	}

	void beginRappel(Object* target, const Coord3D& pos);
	Bool createRopes();
	Bool dropNextPassenger(Int ropeIndex);
	Object* getPotentialRappeller() const;
	void cleanupFinishedRappellers();
	void finishRappel();
	void cancelRappel();

	DropState m_dropState;
	Coord3D m_dropPosition;
	ObjectID m_dropTargetID;
	mutable ObjectID m_requestedExitID;
	Real m_oldPreferredHeight;
	Bool m_preferredHeightAdjusted;
	RopeInfo m_ropes[2];
};
