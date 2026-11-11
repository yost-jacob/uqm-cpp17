#ifndef UQM_CONVERSATION_REPORTS_HPP_
#define UQM_CONVERSATION_REPORTS_HPP_
#include "dialogue/state.hpp"
#include <map>
#include <optional>
namespace uqm {
class PlayerEntity;
class CampaignClock;
// Visit-owned starbase plans. Preparing collects immutable speech and proposed
// effects; only an explicit post-playback commit acknowledges or grants them.
class StarbaseReports {
public:
    StarbaseReports(PlayerEntity &player, CampaignClock &clock, dialogue::Json phraseNames, bool voiced);
    bool available(const std::string &name) const; // Devices or current-visit bulletins; no acknowledgement.
    // Prepare a named report with empty parameters. "hayes.commit" instead
    // takes the returned commit object after playback. Old plan tokens are stale.
    dialogue::Result prepare(const std::string &name, const dialogue::Json &parameters);
    dialogue::Result sellMinerals(); // Sell all cargo exactly once, or leave cargo/RU unchanged on capacity failure.
private:
    struct Plan {
        dialogue::Json speech = dialogue::Json::array();
        std::map<std::string, unsigned, std::less<>> writes;
        unsigned bulletins = 0, gift = 0, ru = 0;
    };
    class Builder;
    PlayerEntity &player_;
    CampaignClock &clock_;
    const dialogue::Json phrases_;
    bool voiced_;
    unsigned bulletins_ = 0;
    unsigned serial_ = 0;
    std::optional<Plan> pending_;
    dialogue::Json bulletinSpeech_ = dialogue::Json::array();
    Plan build(const std::string &, unsigned greeting = 0, unsigned intro = 0) const;
};
}
#endif
