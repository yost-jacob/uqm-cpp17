#ifndef UQM_CONVERSATION_STATE_HPP
#define UQM_CONVERSATION_STATE_HPP
#include "dialogue/legacy_turn.hpp"
namespace uqm {
class PlayerEntity;
class StoryState;
class CampaignClock;
// Borrow the active session's player for one visit. Story reads are const;
// native fleet/event adapters still require that campaign's C binding to be active.
class CampaignDialogueState final : public dialogue::LegacyTurnState {
  public:
    using Report = std::function<dialogue::Result(const std::string &, const dialogue::Json &)>;
    CampaignDialogueState(PlayerEntity &, CampaignClock &, std::string, Callback, Report, dialogue::Json);
    dialogue::Json read(const std::string &, const std::string &) const override;
    dialogue::Result set(const std::string &, const std::string &, const dialogue::Json &) override;
    dialogue::Result setOutcome(const std::string &) override;
    dialogue::Integer randomBelow(dialogue::Integer) override;
    dialogue::Result scheduleEvent(const std::string &, dialogue::Integer, dialogue::Integer,
                                   dialogue::Integer) override;
    dialogue::Result prepareReport(const std::string &, const dialogue::Json &) override;
    dialogue::Integer fuel() const override;
    dialogue::Integer fuelCapacity() const override;
    dialogue::Integer quantity(const std::string &) const override;
    dialogue::Integer availableSlots(const std::string &) const override;
    dialogue::Integer shipCount(const std::string &) const override;
    bool hasShip(const std::string &) const override;
    std::string relationship(const std::string &) const override;
    bool raceAvailable(const std::string &) const override;
    // Explicit captain IDs currently resolve the named Fwiffo resource entry.
    // Validate the crew before recruitment; a full fleet returns count zero.
    dialogue::Result createShip(const std::string &, const std::string &,
                                dialogue::Integer) override;
    dialogue::Result giveShips(const std::string &, dialogue::Integer) override;
    dialogue::Result removeShips(const std::string &, dialogue::Integer) override;
    dialogue::Result setRelationship(const std::string &, const std::string &) override;
    dialogue::Result revealRace(const std::string &) override;
    dialogue::Result configureShip(const std::string &, const dialogue::Json &) override;
    dialogue::Result adjustShip(const std::string &, dialogue::Integer) override;

  private:
    PlayerEntity &player_;
    StoryState &story_;
    CampaignClock &clock_;
    Report report_;
    const dialogue::Json context_;
};
} // namespace uqm
#endif
