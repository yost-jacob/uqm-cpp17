#ifndef UQM_DESIGN_CONVERSATION_STATE_HPP
#define UQM_DESIGN_CONVERSATION_STATE_HPP
#include <cstdint>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
namespace uqm::design::conversation {
using Json = nlohmann::json;
using Integer = std::int64_t;
struct Result {
    std::string code = "ok";
    Json values = Json::object();
};
// Proposed native state-access contract. This is not a second live GameState.
// The session supplies an adapter borrowing its player/galaxy objects. All const
// queries are repeatable and must not mutate state, consume RNG or play media.
// Methods deliberately fail until implemented by an adapter; no silent success.
class State {
    [[noreturn]] static void missing(const char *name) {
        throw std::logic_error(std::string("unimplemented state operation: ") + name);
    }

  public:
    virtual ~State() = default;
    virtual Json read(const std::string &, const std::string &) const {
        missing("read");
    }
    virtual Result set(const std::string &, const std::string &, const Json &) {
        missing("set");
    }
    virtual bool hasShip(const std::string &) const {
        missing("hasShip");
    }
    virtual Integer availableSlots(const std::string &) const {
        missing("availableSlots");
    }
    virtual Integer shipCount(const std::string &) const {
        missing("shipCount");
    }
    virtual Integer fleetValue() const {
        missing("fleetValue");
    }
    virtual std::string relationship(const std::string &) const {
        missing("relationship");
    }
    virtual bool raceAvailable(const std::string &) const {
        missing("raceAvailable");
    }
    virtual bool hasItem(const std::string &) const {
        missing("hasItem");
    }
    virtual Integer quantity(const std::string &) const {
        missing("quantity");
    }
    virtual Integer fuel() const {
        missing("fuel");
    } // Displayed fuel units, not legacy hundredths.
    virtual Integer fuelCapacity() const {
        missing("fuelCapacity");
    }
    virtual Integer balance(const std::string &) const {
        missing("balance");
    }
    virtual Integer equipmentCount(const std::string &) const {
        missing("equipmentCount");
    }
    virtual Integer salvageValue() const {
        missing("salvageValue");
    }
    virtual bool capability(const std::string &) const {
        missing("capability");
    }
    virtual bool hasTechnology(const std::string &) const {
        missing("hasTechnology");
    }
    virtual bool marketAvailable(const std::string &, const std::string &) const {
        missing("marketAvailable");
    }
    virtual bool dateReached(const std::string &, Integer, Integer, bool) const {
        missing("dateReached");
    }
    virtual bool reportAvailable(const std::string &) const {
        missing("reportAvailable");
    }
    virtual Json encounter(const std::string &) const {
        missing("encounter");
    }
    virtual Integer distanceTo(const std::string &) const {
        missing("distanceTo");
    }
    // Defaults are generated when the action executes, never when JSON loads.
    Result createShip(const std::string &type) {
        return createShip(type, nextCaptainName(type), maximumCrew(type));
    }
    virtual Result createShip(const std::string &, const std::string &, Integer) {
        missing("createShip");
    }
    virtual std::string nextCaptainName(const std::string &) {
        missing("nextCaptainName");
    }
    virtual Integer maximumCrew(const std::string &) const {
        missing("maximumCrew");
    }
    virtual Result giveShips(const std::string &, Integer) {
        missing("giveShips");
    }
    Result removeShips(const std::string &type) {
        return removeShips(type, shipCount(type));
    }
    virtual Result removeShips(const std::string &, Integer) {
        missing("removeShips");
    }
    virtual Result setShipCrew(const std::string &, Integer, const std::string &) {
        missing("setShipCrew");
    }
    Result transferItem(const std::string &item) {
        return transferItem(item, 1);
    }
    virtual Result transferItem(const std::string &, Integer) {
        missing("transferItem");
    }
    virtual Result adjustShip(const std::string &, Integer) {
        missing("adjustShip");
    }
    virtual Result configureShip(const std::string &, const Json &) {
        missing("configureShip");
    }
    virtual Result adjustCurrency(const std::string &, Integer) {
        missing("adjustCurrency");
    }
    // Purchase/sale validate eligibility, capacity and payment together. Failure
    // codes are data for an explicit decision node; these functions play no speech.
    virtual Result purchase(const std::string &, const std::string &, const Json &) {
        missing("purchase");
    }
    virtual Result sell(const std::string &, const std::string &, const Json &) {
        missing("sell");
    }
    virtual Result prepareRescue(const std::string &) {
        missing("prepareRescue");
    }
    virtual Result acceptRescue(const std::string &) {
        missing("acceptRescue");
    }
    virtual Result stripShip(const std::string &) {
        missing("stripShip");
    }
    virtual Result grantTechnology(const std::string &) {
        missing("grantTechnology");
    }
    virtual Result setRelationship(const std::string &, const std::string &) {
        missing("setRelationship");
    }
    virtual Result revealRace(const std::string &) {
        missing("revealRace");
    }
    virtual Result scheduleEvent(const std::string &, Integer, Integer, Integer) {
        missing("scheduleEvent");
    }
    virtual Result rememberDate(const std::string &) {
        missing("rememberDate");
    }
    // Reports prepare utterance data and perform explicitly documented bookkeeping
    // (bulletins/devices); they do not start audio or draw a dialog themselves.
    virtual Result prepareReport(const std::string &, const Json &) {
        missing("prepareReport");
    }
    virtual Result setOutcome(const std::string &) {
        missing("setOutcome");
    }
    virtual Integer randomBelow(Integer) {
        missing("randomBelow");
    }
};
} // namespace uqm::design::conversation
#endif
