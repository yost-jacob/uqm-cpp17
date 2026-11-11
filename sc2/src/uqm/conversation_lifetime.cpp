#include "controls.h"
#include "conversation.hpp"
#include "game_state.hpp"
#include "libs/inplib.h"
#include "libs/sndlib.h"
#include "libs/sound/trackplayer.h"
#include "libs/threadlib.h"
#include "settings.h"
#include "setup.h"
#include <stdexcept>
using namespace uqm;
uqm::ConversationRuntime::ConversationRuntime(GameSession &session, const LOCDATA &configuration)
    : session_(session), story_(session.player().story()), resources_(configuration),
      data_(resources_.script.data), animations_(data_) {
    // Graph-owned scripts need no native turn callback. Optional post/uninit
    // hooks retain only the remaining native encounter policy, never ownership.
    if (data_.NumAnimations > MAX_ANIMATIONS)
        throw std::out_of_range("conversation animation capacity exceeded");
    resources_.script.owner = this;
    if (data_.ConversationPhrasesRes && std::string_view(data_.ConversationPhrasesRes) == "comm.melnorme.dialogue")
        previousStatusMode_ = static_cast<int>(SetStatusMessageMode(SMM_CREDITS));
}
uqm::ConversationRuntime::~ConversationRuntime() {
    stopPlayback();
    if (previousStatusMode_) {
        SetStatusMessageMode(static_cast<StatMsgMode>(*previousStatusMode_));
        DrawStatusMessage(nullptr);
    }
    if (started_) {
        if (fontBound_) {
            SetContext(SpaceContext);
            SetContextFont(oldSpaceFont_);
        }
        SetContext(oldContext_);
        SetMenuSounds(oldArrows_, oldSelect_);
    }
    // Script bindings are released before resources_'s destructor frees data.
}
void uqm::ConversationRuntime::stopPlayback() {
    if (!playback_)
        return;
    playback_ = false;
    SetContext(SpaceContext);
    FlushColorXForms();
    ClearSubtitles();
    StopMusic();
    StopSound();
    StopTrack();
    SleepThreadUntil(FadeMusic(NORMAL_VOLUME, 0) + ONE_SECOND / 60);
}
bool uqm::ConversationRuntime::exiting() const {
    return QuitPosted || session_.menuExit().has_value() || (GLOBAL(CurrentActivity) & CHECK_ABORT);
}
void uqm::ConversationRuntime::finishScript() {
    if (scriptFinished_)
        return;
    scriptFinished_ = true;
    if (!exiting() && !(GLOBAL(CurrentActivity) & CHECK_LOAD)) {
        bool salvage = false;
        if (graph_) {
            const auto found = graph_->memory().local.find("attempted_salvage");
            salvage = found != graph_->memory().local.end() && found->second == true;
        }
        if (data_.ConversationPhrasesRes) completeConversationPolicy(story_, data_.ConversationPhrasesRes, getSegue() == Segue_hostile, salvage);
    }
    if (data_.post_encounter_func && !exiting() && !(GLOBAL(CurrentActivity) & CHECK_LOAD))
        data_.post_encounter_func();
    if (data_.uninit_encounter_func)
        data_.uninit_encounter_func();
}
void uqm::ConversationRuntime::finishWithoutDialogue() {
    if (started_ || scriptFinished_)
        throw std::logic_error("conversation already completed");
    binding_.emplace(resources_.script);
    finishScript();
}
void uqm::ConversationRuntime::enableTalking(bool enable) {
    if (enable)
        data_.AlienTalkDesc.AnimFlags &= ~PAUSE_TALKING;
    else
        data_.AlienTalkDesc.AnimFlags |= PAUSE_TALKING;
}
