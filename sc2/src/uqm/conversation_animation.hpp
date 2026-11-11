#ifndef UQM_CONVERSATION_ANIMATION_HPP_
#define UQM_CONVERSATION_ANIMATION_HPP_
#include "libs/timelib.h"
#include "commanim.h"
#include "globdata.h"
#include <array>
namespace uqm {
typedef enum
{
	DOWN_DIR = -1, // Animation indices are decreasing
	NO_DIR = 0,
	UP_DIR = 1,    // Animation indices are increasing
} ANIM_DIR;

typedef enum
{
	PICTURE_ANIM,
			// Parts of a picture are replaced
	COLOR_ANIM
			// Colormap tricks on a picture
} ANIM_TYPE;

// Describes an active animation.
struct SEQUENCE
{
	ANIMATION_DESC *ADPtr;
	DWORD Alarm;
	ANIM_DIR Direction;
	COUNT CurIndex;
	COUNT NextIndex;
	COUNT FramesLeft;
	ANIM_TYPE AnimType;
	BOOLEAN Change;
};

typedef struct SEQUENCE SEQUENCE;

inline bool
haveTalkingAnim(const LOCDATA &data)
{
	return data.AlienTalkDesc.NumFrames > 0;
}

inline bool
haveTransitionAnim(const LOCDATA &data)
{
	return data.AlienTransitionDesc.NumFrames > 0;
}

inline bool
wantTalkingAnim(const LOCDATA &data)
{
	return !(data.AlienTalkDesc.AnimFlags & PAUSE_TALKING);
}

inline void
setRunTalkingAnim(LOCDATA &data)
{
	data.AlienTalkDesc.AnimFlags |= WAIT_TALKING;
}

inline void
clearRunTalkingAnim(LOCDATA &data)
{
	data.AlienTalkDesc.AnimFlags &= ~WAIT_TALKING;
}

inline bool
runningTalkingAnim(const LOCDATA &data)
{
	return (data.AlienTalkDesc.AnimFlags & WAIT_TALKING);
}

inline void
setRunIntroAnim(LOCDATA &data)
{
	data.AlienTransitionDesc.AnimFlags |= TALK_INTRO;
}

inline bool
runningIntroAnim(const LOCDATA &data)
{
	return (data.AlienTransitionDesc.AnimFlags & TALK_INTRO);
}

inline void
setStopTalkingAnim(LOCDATA &data)
{
	data.AlienTalkDesc.AnimFlags |= TALK_DONE;
}

inline void
clearStopTalkingAnim(LOCDATA &data)
{
	data.AlienTalkDesc.AnimFlags &= ~TALK_DONE;
}

inline bool
signaledStopTalkingAnim(const LOCDATA &data)
{
	return data.AlienTalkDesc.AnimFlags & TALK_DONE;
}

// One conversation owns these sequences. Internal pointers refer into this
// object's array/descriptors, so it must never be copied or moved.
class ConversationAnimations {
public:
    explicit ConversationAnimations(LOCDATA &data) noexcept : data_(data) {}
    ConversationAnimations(const ConversationAnimations &) = delete;
    ConversationAnimations &operator=(const ConversationAnimations &) = delete;
    void InitCommAnimations();
    BOOLEAN redraw() { return DrawAlienFrame(Sequences.data(), TotalSequences, TRUE); }
    BOOLEAN ProcessCommAnimations(BOOLEAN fullRedraw, BOOLEAN paused);
    BOOLEAN DrawAlienFrame(SEQUENCE *sequences, COUNT count, BOOLEAN fullRedraw);
private:
    DWORD randomFrameRate(SEQUENCE *pSeq);
    DWORD randomRestartRate(SEQUENCE *pSeq);
    COUNT randomFrameIndex(SEQUENCE *pSeq, COUNT from);
    void SetupAmbientSequences(SEQUENCE *pSeq, COUNT Num);
    void SetupTalkSequence(SEQUENCE *pSeq, ANIMATION_DESC *ADPtr);
    BOOLEAN animAtNeutralIndex(SEQUENCE *pSeq);
    BOOLEAN conflictsWithTalkingAnim(SEQUENCE *pSeq);
    void ProcessColormapAnims(SEQUENCE *pSeq, COUNT Num);
    BOOLEAN AdvanceAmbientSequence(SEQUENCE *pSeq);
    void ResetSequence(SEQUENCE *pSeq);
    void AdvanceTalkingSequence(SEQUENCE *pSeq, DWORD ElapsedTicks);
    BOOLEAN AdvanceTransitSequence(SEQUENCE *pSeq, DWORD ElapsedTicks);
    LOCDATA &data_;
    TimeCount LastTime = 0;
    std::array<SEQUENCE, MAX_ANIMATIONS + 2> Sequences{};
    DWORD ActiveMask = 0;
    ANIMATION_DESC TalkDesc{}, TransitDesc{};
    SEQUENCE *Talk = nullptr, *Transit = nullptr;
    COUNT FirstAmbient = 0, TotalSequences = 0;
};
} // namespace uqm
#endif
