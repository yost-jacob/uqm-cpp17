//Copyright Paul Reiche, Fred Ford. 1992-2002

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "combat_mode.hpp"
#include "game_state.hpp"
#include "encounter_legacy.hpp"
#include "sis_legacy.hpp"
#include "battle.h"
#include "battlecontrols.h"
#include "intel.h"
#include "nameref.h"
#include "controls.h"
#include "globdata.h"
#include "resinst.h"
#include "setup.h"
#include "sounds.h"
#include "libs/inplib.h"
#include "libs/log.h"
#include "libs/misc.h"

extern "C" UWORD nth_frame;

namespace uqm {
namespace {
class CampaignCombatPreparation final : public CombatPreparation {
public:
    explicit CampaignCombatPreparation(GameSession &session) : session_(session), story_(session.player().story()) {}
    bool prepare(SimulationEngine &) override;
    void complete() override {
        if (session_.activity() & CHECK_ABORT)
            detail::SISAccess::record(session_.player().sis()).CrewEnlisted = static_cast<COUNT>(~0u);
    }
private:
    struct BattleCleanup {
        explicit BattleCleanup(ACTIVITY &owner) : owner(owner), activity(owner) {}
        ACTIVITY &owner;
        ACTIVITY activity;
        SOUND savedSounds = GameSounds;
        UWORD savedFrame = nth_frame;
        BYTE savedControl = PlayerControl[0];
        InputContext *savedPlayerInput = nullptr;
        bool cyborgInput = false;
        bool soundLoaded = false;
        ~BattleCleanup() {
            if (soundLoaded) {
                DestroySound(ReleaseSound(GameSounds));
                GameSounds = savedSounds;
            }
            if (cyborgInput) {
                nth_frame = savedFrame;
                PlayerControl[0] = savedControl;
                ClearPlayerInput(0);
                PlayerInput[0] = savedPlayerInput;
            }
            owner = activity;
        }
    };
    GameSession &session_;
    StoryState &story_;
    std::optional<BattleCleanup> bindings_;
};
bool CampaignCombatPreparation::prepare(SimulationEngine &) {
    auto &cleanup = bindings_.emplace(session_.activity());
    story_.set(StoryFlag::BattleSegue, true);
    session_.activity() = MAKE_WORD(
        LOBYTE(cleanup.activity) == IN_LAST_BATTLE ? IN_LAST_BATTLE : IN_ENCOUNTER, 0);
    WaitForSoundEnd(TFBSOUND_WAIT_ALL);
    if (detail::encounterLegacy(session_.player()).flags & CYBORG_ENABLED) {
        auto speed = static_cast<BYTE>((detail::encounterLegacy(session_.player()).flags & COMBAT_SPEED_MASK) >> COMBAT_SPEED_SHIFT);
        if (speed == 1) speed = 0; // Normal speed.
        else if (speed == 2) ++speed; // Four times speed: skip three of four frames.
        else speed = static_cast<BYTE>(~0u); // Maximum speed without rendering.
        nth_frame = MAKE_WORD(1, speed);
        PlayerControl[0] = CYBORG_CONTROL | AWESOME_RATING;
        cleanup.savedPlayerInput = PlayerInput[0];
        cleanup.cyborgInput = true;
        PlayerInput[0] = nullptr;
        if (!SetPlayerInput(0)) {
            throw std::runtime_error("could not initialize cyborg input");
        }
    }
    GameSounds = CaptureSound(LoadSound(GAME_SOUNDS));
    cleanup.soundLoaded = true;
    if (!GameSounds) throw std::runtime_error("could not load combat sounds");
    return true;
}
}
std::unique_ptr<CombatPreparation> createCampaignCombatPreparation(GameSession &session) {
    return std::make_unique<CampaignCombatPreparation>(session);
}
}
