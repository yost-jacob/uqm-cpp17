#include "conversation_cues.hpp"
#include "colors.h"
#include "comm.h"
#include "commglue.h"
#include "libs/reslib.h"
namespace uqm {
namespace {
// The native track player invokes these on the conversation thread. They borrow
// active media resources and never write story state, consume RNG or dispatch.
void alternateMap(CallbackArg) {
    auto &map = GetConversationData()->AlienColorMap;
    map = SetAbsColorMapIndex(map, 1);
}
void fade(unsigned index, unsigned duration) {
    XFormColorMap(
        GetColorMapAddress(SetAbsColorMapIndex(GetConversationData()->AlienColorMap, index)),
        duration);
}
void alternateQuarter(CallbackArg) {
    fade(1, ONE_SECOND / 4);
}
void alternateHalf(CallbackArg) {
    fade(1, ONE_SECOND / 2);
}
void blueMap(CallbackArg) { fade(2, ONE_SECOND / 2); }
void alternateSecond(CallbackArg) {
    fade(1, ONE_SECOND);
}
void defaultHalf(CallbackArg) {
    fade(0, ONE_SECOND / 2);
}
void talkingOff(CallbackArg) {
    EnableTalkingAnim(FALSE);
}
void talkingOn(CallbackArg) {
    EnableTalkingAnim(TRUE);
}
void commanderPower(CallbackArg) {
    GetConversationData()->AlienAmbientArray[2].AnimFlags |= ANIM_DISABLED;
    fade(0, ONE_SECOND / 2);
}
void chmmrEmerging(CallbackArg) {
    SetCommIntroMode(CIM_FADE_IN_SCREEN, ONE_SECOND * 2);
}
// Speaker state is the active descriptor's current talk profile, not a global
// LastAlien variable. Reset reproduces the neutral-to-Zoq start of each batch.
void zoqSpeaker(bool zoq, bool reset) {
    auto &data = *GetConversationData();
    const auto index = zoq ? 18 : 29;
    if (!reset && data.AlienTalkDesc.StartIndex == index)
        return;
    if (!reset)
        data.AlienTransitionDesc.AnimFlags |= TALK_DONE;
    data.AlienTransitionDesc.AnimFlags |= TALK_INTRO;
    data.AlienTransitionDesc.StartIndex = zoq ? 23 : 26;
    data.AlienTalkDesc.StartIndex = index;
    data.AlienTalkDesc.NumFrames = zoq ? 5 : 2;
    if (zoq)
        data.AlienAmbientArray[1].AnimFlags &= ~WAIT_TALKING;
    else
        data.AlienAmbientArray[1].AnimFlags |= WAIT_TALKING;
    const auto left = TEXT_X_OFFS + ((SIS_TEXT_WIDTH >> 1) >> 1);
    data.AlienTextBaseline.x = zoq ? left : SIS_SCREEN_WIDTH - left;
    data.AlienTextBaseline.y = 24;
    data.AlienTextFColor = WHITE_COLOR;
    data.AlienTextBColor = BLACK_COLOR;
}
void zoqReset(CallbackArg) {
    zoqSpeaker(true, true);
}
void zoq(CallbackArg) {
    zoqSpeaker(true, false);
}
void pik(CallbackArg) {
    zoqSpeaker(false, false);
}
void petStrobe(CallbackArg) {
    // Ambient slot 16 is the one-shot strobe in the Talking Pet's native asset profile.
    GetConversationData()->AlienAmbientArray[16].AnimFlags &= ~ANIM_DISABLED;
}
} // namespace
CallbackFunction conversationCue(const std::string &resource, const std::string &name) {
    struct Cue {
        const char *resource, *name;
        CallbackFunction callback;
    };
    static const Cue cues[] = {
        {"comm.melnorme.dialogue", "melnorme.purple", alternateHalf},
        {"comm.melnorme.dialogue", "melnorme.blue", blueMap},
        {"comm.melnorme.dialogue", "melnorme.yellow", defaultHalf},
        {"comm.starbase.dialogue", "starbase.scientist", talkingOff},
        {"comm.starbase.dialogue", "starbase.commander", talkingOn},
        {"comm.commander.dialogue", "commander.emergency_map", alternateMap},
        {"comm.commander.dialogue", "commander.restore_power", commanderPower},
        {"comm.zoqfotpik.dialogue", "zoqfot.reset", zoqReset},
        {"comm.zoqfotpik.dialogue", "zoqfot.zoq", zoq},
        {"comm.zoqfotpik.dialogue", "zoqfot.pik", pik},
        {"comm.chmmr.dialogue", "chmmr.emerging", chmmrEmerging},
        {"comm.chmmr.dialogue", "chmmr.shielded_map", alternateMap},
        {"comm.vux.dialogue", "vux.beast_escape", alternateQuarter},
        {"comm.talkingpet.dialogue", "talkpet.strobe", petStrobe},
        {"comm.orz.dialogue", "orz.hostile_map", alternateMap},
        {"comm.orz.dialogue", "orz.anger", alternateHalf},
        {"comm.orz.dialogue", "orz.talking_off", talkingOff},
        {"comm.orz.dialogue", "orz.talking_on", talkingOn},
        {"comm.syreen.dialogue", "syreen.lights_off", alternateSecond},
        {"comm.syreen.dialogue", "syreen.lights_on", defaultHalf},
        {"comm.safeones.dialogue", "spahome.council_map", alternateMap},
        {"comm.safeones.dialogue", "spahome.council_fade", alternateQuarter},
    };
    for (const auto &cue : cues)
        if (resource == cue.resource && name == cue.name)
            return cue.callback;
    throw std::invalid_argument("unknown conversation presentation cue: " + name);
}
std::vector<CallbackFunction> conversationCues(const std::string &resource,
                                               const dialogue::Json &names) {
    if (names.is_string())
        return {conversationCue(resource, names.get<std::string>())};
    if (!names.is_array())
        throw std::invalid_argument("media cues must be a name or array of names");
    std::vector<CallbackFunction> callbacks;
    callbacks.reserve(names.size());
    for (const auto &name : names)
        callbacks.push_back(conversationCue(resource, name.get<std::string>()));
    return callbacks;
}
} // namespace uqm
