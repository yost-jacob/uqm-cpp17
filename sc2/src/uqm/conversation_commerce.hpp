#ifndef UQM_CONVERSATION_COMMERCE_HPP_
#define UQM_CONVERSATION_COMMERCE_HPP_
#include "dialogue/state.hpp"
#include "sis.h"
#include <optional>
namespace uqm {
class PlayerEntity;
class GameSession;
class RandomStream;
class CampaignClock;
// One conversation's offers. Borrows its session and RNG until the visit ends;
// queries never mutate, consume RNG or present media. Plans expire with this owner.
class ConversationCommerce {
public:
    ConversationCommerce(GameSession &session, RandomStream &random, dialogue::Json definitions);
    // Melnorme technology/information/category availability; no price/payment effects.
    bool marketAvailable(const std::string &market, const std::string &category) const;
    dialogue::Integer unreportedRainbows() const; // Discovered count minus already sold count, floored at zero.
    // Fixed items take quantity 1. Melnorme fuel takes a positive displayed
    // quantity or "fill"; failure codes leave payment and delivery unapplied.
    dialogue::Result purchase(const std::string &market, const std::string &offer, const dialogue::Json &quantity);
    // Melnorme sales take "all"; Druuge barter takes "regular" or "sphere".
    // Returned speech describes this committed exchange; replay cannot resell it.
    dialogue::Result sell(const std::string &market, const std::string &asset, const dialogue::Json &quantity);
    dialogue::Result report(const std::string &name, const dialogue::Json &parameters);
    dialogue::Integer salvageValue() const; // Removable equipment in MODULE_COST_SCALE units.
    // "melnorme" selects the registered policy. Repeated preparation reuses the offer.
    dialogue::Result prepareRescue(const std::string &policy);
    dialogue::Result acceptRescue(const std::string &token); // Token belongs to this visit; validate current equipment before applying.
    dialogue::Result stripShip(const std::string &policy); // "melnorme.penance"; protect required crew pods.
private:
    GameSession &session_;
    RandomStream &random_;
    PlayerEntity &player_;
    CampaignClock &clock_;
    const dialogue::Json definitions_;
    unsigned slavery_ = 0;
    struct RescueOffer { SIS_STATE original, stripped; unsigned fuel; bool accepted = false; };
    std::optional<RescueOffer> rescue_;
    dialogue::Result sellDruuge(const std::string &, bool sphere);
    const dialogue::Json *nextTechnology() const;
    bool hasTechnology(const dialogue::Json &) const;
    void grantTechnology(const dialogue::Json &);
};
}
#endif
