// Reborn: Generals Online-style observer statistics and notifications, adapted for Reborn factions and powers.
#pragma once

#include "Common/GameCommon.h"
#include "Common/Science.h"

class Player;
class SpecialPowerTemplate;

//-------------------------------------------------------------------------------------------------
/** Reborn: Keep spectator-only display resources and event history outside simulation snapshots. */
//-------------------------------------------------------------------------------------------------
class RebornObserverOverlay
{
public:
    RebornObserverOverlay();
    ~RebornObserverOverlay();
    void reset();
    void draw();
    void toggle();
    void notifyScience(Player* player, ScienceType science);
    void notifyPower(Player* player, const SpecialPowerTemplate* power);
private:
    struct State;
    State* m_state;
    RebornObserverOverlay(const RebornObserverOverlay&) = delete;
    RebornObserverOverlay& operator=(const RebornObserverOverlay&) = delete;
};
