#include "campaign_encounters.hpp"
#include "game_state.hpp"
#include <exception>
extern "C" {
COUNT CampaignEncounterCount(void) { return static_cast<COUNT>(uqm::gameSession().encounters().size()); }
ENCOUNTER *AppendCampaignEncounter(void) {
    try {
        auto &entries = uqm::gameSession().encounters();
        const auto entry = entries.insert();
        return entry == entries.end() ? nullptr : &*entry;
    } catch (...) { std::terminate(); }
}
void ClearCampaignEncounters(void) { uqm::gameSession().encounters().clear(); }
void SelectCampaignEncounter(COUNT index) {
    auto &entries = uqm::gameSession().encounters();
    entries.select(index < entries.size() ? entries.begin() + index : entries.end());
}
ENCOUNTER *CampaignEncounterForSave(COUNT index) {
    auto &entries = uqm::gameSession().encounters();
    if (index >= entries.size()) return nullptr;
    const auto current = entries.current();
    if (current == entries.end()) return &entries.at(index);
    if (index == 0) return &*current;
    const auto selected = static_cast<std::size_t>(current - entries.begin());
    return &entries.at(index <= selected ? index - 1 : index);
}
}
