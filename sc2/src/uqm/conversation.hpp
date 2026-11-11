#ifndef UQM_CONVERSATION_HPP_
#define UQM_CONVERSATION_HPP_
#include "conversation_animation.hpp"
#include "conversation_resources.hpp"
#include "conversation_story.hpp"
#include "dialogue/legacy_turn.hpp"
#include "game_mode.hpp"
#include "sounds.h"
#include "libs/sound/trackplayer.h"
#include <optional>
#include <deque>
namespace uqm {
class StoryState;
class GameSession;
class ConversationRuntime;
inline constexpr unsigned MaxResponses = 8;
inline constexpr int SLIDER_Y = 107, SLIDER_HEIGHT = 15;
typedef struct response_entry
{
	RESPONSE_REF response_ref;
	TEXT response_text;
	RESPONSE_FUNC response_func;
} RESPONSE_ENTRY;

typedef struct encounter_state
{
	BOOLEAN (*InputFunc) (struct encounter_state *pES);
    ConversationRuntime *owner;

	COUNT Initialized;
	TimeCount NextTime; // framerate control
	BYTE num_responses;
	BYTE cur_response;
	BYTE top_response;
	std::array<RESPONSE_ENTRY, MaxResponses> response_list;

	std::array<UNICODE, 1024> phrase_buf;
} ENCOUNTER_STATE;

typedef struct talking_state
{
	// Fields required by DoInput()
	BOOLEAN (*InputFunc) (struct talking_state *);
    ConversationRuntime *owner;

	TimeCount NextTime;  // framerate control
	COUNT waitTrack;
	bool rewind;
	bool seeking;
	bool ended;

} TALKING_STATE;

typedef struct summary_state
{
	// standard state required by DoInput
	BOOLEAN (*InputFunc) (struct summary_state *pSS);
    ConversationRuntime *owner;

	// extended state
	BOOLEAN Initialized;
	BOOLEAN PrintNext;
	SUBTITLE_REF NextSub;
	const UNICODE *LeftOver;

} SUMMARY_STATE;

typedef struct last_replay_state
{
	// Fields required by DoInput()
	BOOLEAN (*InputFunc) (struct last_replay_state *);
    ConversationRuntime *owner;

	TimeCount NextTime; // framerate control
	TimeCount TimeOut;

} LAST_REPLAY_STATE;


// One synchronous dialogue visit. story state/session outlive the mode; script
// configuration is copied by value and loaded assets belong solely to this owner.
class ConversationRuntime {
public:
    ConversationRuntime(GameSession &session, const LOCDATA &configuration);
    ~ConversationRuntime();
    ConversationRuntime(const ConversationRuntime &) = delete;
    ConversationRuntime &operator=(const ConversationRuntime &) = delete;
    bool belongsTo(const GameSession &session) const noexcept { return &session_ == &session; }
    void run();
    void finishWithoutDialogue(); // Attack path: retain post/uninit script timing.
    std::unique_ptr<ModeSuspension> suspend();
    void resume();
    void setIntro(CommIntroMode mode, TimeCount duration) noexcept { curIntroMode = mode; fadeTime = duration; }
    void AlienTalkSegue(COUNT wait_track);
    void DoResponsePhrase(RESPONSE_REF response, RESPONSE_FUNC handler, UNICODE *constructed);
    void enableTalking(bool enable);
private:
    int _count_lines(TEXT *pText);
    COORD add_text(int status, TEXT *pTextIn);
    void DrawSISComWindow();
    void RefreshResponses(ENCOUNTER_STATE *pES);
    void FeedbackPlayerPhrase(UNICODE *pStr);
    void InitSpeechGraphics();
    void UpdateSpeechGraphics();
    void UpdateAnimations(bool paused);
    void UpdateCommGraphics();
    BOOLEAN DoTalkSegue(TALKING_STATE *pTS);
    void runCommAnimFrame();
    BOOLEAN TalkSegue(COUNT wait_track);
    void CommIntroTransition();
    BOOLEAN DoConvSummary(SUMMARY_STATE *pSS);
    void SelectResponse(ENCOUNTER_STATE *pES);
    void SelectConversationSummary(ENCOUNTER_STATE *pES);
    void SelectReplay(ENCOUNTER_STATE *pES);
    void PlayerResponseInput(ENCOUNTER_STATE *pES);
    BOOLEAN DoLastReplay(LAST_REPLAY_STATE *pLRS);
    BOOLEAN DoCommunication(ENCOUNTER_STATE *pES);
    void RedrawSubtitles();
    void ClearSubtitles();
    void CheckSubtitles();
    bool exiting() const; // Pending session exits stop every dialogue input loop.
    void queueGraphSpeech();
    COUNT graphPhrase(const std::string &name) const;
    void loadGraph(); // Validate the live JSON before invoking any story callbacks.
    dialogue::Result runNativeTurn(std::optional<std::size_t> selection);
    bool finishGraphSpeech(); // Playback completion, independent of replay controls.
    void finishScript();
    void stopPlayback();
    GameSession &session_;
    StoryState &story_;
    ConversationResources resources_;
    LOCDATA &data_;
    ConversationAnimations animations_;
    ENCOUNTER_STATE input_{};
    BOOLEAN TalkingFinished = FALSE;
    CommIntroMode curIntroMode = CIM_DEFAULT;
    TimeCount fadeTime = 0, nextSpeechFrame_ = 0, nextAnimationFrame_ = 0;
    BOOLEAN clear_subtitles = FALSE;
    TEXT SubtitleText{};
    const UNICODE *last_subtitle = nullptr;
    COORD last_baseline = 0;
    RECT window_{};
    FONT oldSpaceFont_ = nullptr;
    CONTEXT oldContext_ = nullptr;
    MENU_SOUND_FLAGS oldArrows_{}, oldSelect_{};
    bool started_ = false, playback_ = false, fontBound_ = false, scriptFinished_ = false;
    std::deque<std::string> graphSpeechInsertions_; // Stable through retained replay tracks.
    std::vector<std::string> graphResponseIds_, graphResponseText_;
    std::unordered_map<std::string, COUNT> graphPhrases_;
    bool nativeGraph_ = true, myconMadeChoice_ = false;
    std::optional<MyconRamblePolicy> myconRamble_;
    std::unique_ptr<dialogue::LegacyTurnState> graphState_;
    std::unique_ptr<dialogue::Conversation> graph_; // Destroy before borrowed state/resources.
    std::optional<ConversationBinding> binding_; // Unbind before owned script storage dies.
};
// The child returns completion/session exit; EncounterRuntime selects combat.
std::unique_ptr<GameMode> createConversationMode(GameSession &session,
        std::unique_ptr<ConversationRuntime> conversation);
} // namespace uqm
#endif
