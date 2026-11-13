#ifndef UQM_CONVERSATION_STATE_HPP
#define UQM_CONVERSATION_STATE_HPP
#include "dialogue/live_definition.hpp"
#include "conversation_commerce.hpp"
#include "conversation_reports.hpp"
namespace uqm {
class PlayerEntity;
class GameSession;
class RandomStream;
class StoryState;
class CampaignClock;
// Borrow this session and visit-owned stream until the graph is destroyed.
// Story queries are const. Remaining C fleet/refit/presentation calls select
// this owner in a scoped compatibility binding and restore the caller.
class CampaignDialogueState final : public dialogue::State {
  public:
    using Report = std::function<dialogue::Result(const std::string &, const dialogue::Json &)>;
    CampaignDialogueState(GameSession &, RandomStream &, Report, dialogue::Json);
    dialogue::Json read(const std::string &, const std::string &) const override;
    dialogue::Result set(const std::string &, const std::string &, const dialogue::Json &) override;
    dialogue::Integer storyMaximum(const std::string &) const override;
    dialogue::Result setStoryBatch(const std::vector<std::pair<std::string, dialogue::Json>> &) override;
    void validateInventoryKey(const std::string &) const override;
    dialogue::Result transact(const InventoryTransaction &) override;
    bool hasItem(const std::string &) const override;
    dialogue::Integer balance(const std::string &) const override;
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
    bool capability(const std::string &) const override;
    bool reportAvailable(const std::string &) const override;
    bool marketAvailable(const std::string &, const std::string &) const override;
    dialogue::Integer salvageValue() const override;
    dialogue::Result purchase(const std::string &, const std::string &, const dialogue::Json &) override;
    dialogue::Result sell(const std::string &, const std::string &, const dialogue::Json &) override;
    dialogue::Result prepareRescue(const std::string &) override;
    dialogue::Result acceptRescue(const std::string &) override;
    dialogue::Result stripShip(const std::string &) override;
    dialogue::Result adjustShip(const std::string &, dialogue::Integer) override;

  private:
    GameSession &session_;
    RandomStream &random_;
    PlayerEntity &player_;
    StoryState &story_;
    CampaignClock &clock_;
    Report report_;
    const dialogue::Json context_;
    ConversationCommerce commerce_;
    StarbaseReports reports_;
};
} // namespace uqm
#endif
