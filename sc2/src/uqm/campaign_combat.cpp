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
    explicit CampaignCombatPreparation(StoryState &story) : story_(story) {}
    bool prepare() override;
    void complete() override {
        if (GLOBAL(CurrentActivity) & CHECK_ABORT)
            GLOBAL_SIS(CrewEnlisted) = static_cast<COUNT>(~0u);
    }
private:
    struct BattleCleanup {
        ACTIVITY activity = GLOBAL(CurrentActivity);
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
            GLOBAL(CurrentActivity) = activity;
        }
    };
    StoryState &story_;
    std::optional<BattleCleanup> bindings_;
};
bool CampaignCombatPreparation::prepare() {
    auto &cleanup = bindings_.emplace();
    story_.set(StoryFlag::BattleSegue, true);
    GLOBAL(CurrentActivity) = MAKE_WORD(
        LOBYTE(cleanup.activity) == IN_LAST_BATTLE ? IN_LAST_BATTLE : IN_ENCOUNTER, 0);
    WaitForSoundEnd(TFBSOUND_WAIT_ALL);
    if (GLOBAL(glob_flags) & CYBORG_ENABLED) {
        auto speed = static_cast<BYTE>((GLOBAL(glob_flags) & COMBAT_SPEED_MASK) >> COMBAT_SPEED_SHIFT);
        if (speed == 1) speed = 0; // Normal speed.
        else if (speed == 2) ++speed; // Four times speed: skip three of four frames.
        else speed = static_cast<BYTE>(~0u); // Maximum speed without rendering.
        nth_frame = MAKE_WORD(1, speed);
        PlayerControl[0] = CYBORG_CONTROL | AWESOME_RATING;
        cleanup.savedPlayerInput = PlayerInput[0];
        cleanup.cyborgInput = true;
        PlayerInput[0] = nullptr;
        if (!SetPlayerInput(0)) {
            log_add(log_Fatal, "Could not set cyborg player input.");
            explode(); // Preserve the existing fatal initialization policy.
        }
    }
    GameSounds = CaptureSound(LoadSound(GAME_SOUNDS));
    cleanup.soundLoaded = true;
    return true;
}
}
std::unique_ptr<CombatPreparation> createCampaignCombatPreparation(StoryState &story) {
    return std::make_unique<CampaignCombatPreparation>(story);
}
}
