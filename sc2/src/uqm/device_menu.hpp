#ifndef UQM_DEVICE_MENU_HPP_
#define UQM_DEVICE_MENU_HPP_
#include "planets/planets.h"
#include "menustat.h"
#include "campaign_encounters.hpp"
#include <vector>
namespace uqm {
class StoryState;
// Ordered inventory for the supplied player; detached, with no buffer capacity
// contract. Legacy save callers copy the prefix that fits their fixed buffer.
std::vector<BYTE> inventoryDevices(const StoryState &story);
enum class DeviceStatus { Failure, Success, SuccessNoSound };
// The caller owns story state/system for the complete synchronous menu visit.
// Omitting system selects navigation context. Menu callbacks borrow this owner
// through MENU_STATE userdata, never through the process's active story state.
class DeviceMenu {
public:
    DeviceMenu(StoryState &story, CampaignEncounters &encounters) noexcept
        : story_(story), encounters_(&encounters) {}
    DeviceMenu(StoryState &story, SOLARSYS_STATE &system);
    BOOLEAN run(); // Legacy result: whether the parent menu may continue.
private:
    bool useCaster();
    DeviceStatus invoke(BYTE device);
    static BOOLEAN manipulate(MENU_STATE *menu);
    bool inOrbit() const;
    StoryState &story_;
    CampaignEncounters *encounters_ = nullptr; // Supplied by navigation; system menus use groups.
    SOLARSYS_STATE *system_ = nullptr; // Optional borrowed exploration descriptor.
};
} // namespace uqm
#endif
