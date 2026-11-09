#ifndef UQM_DEVICE_MENU_HPP_
#define UQM_DEVICE_MENU_HPP_
#include "planets/planets.h"
#include "menustat.h"
#include <vector>
namespace uqm {
class CampaignState;
// Ordered inventory for the supplied player; detached, with no buffer capacity
// contract. Legacy save callers copy the prefix that fits their fixed buffer.
std::vector<BYTE> inventoryDevices(const CampaignState &campaign);
enum class DeviceStatus { Failure, Success, SuccessNoSound };
// The caller owns campaign/system for the complete synchronous menu visit.
// Omitting system selects navigation context. Menu callbacks borrow this owner
// through MENU_STATE userdata, never through the process's active campaign.
class DeviceMenu {
public:
    explicit DeviceMenu(CampaignState &campaign) noexcept : campaign_(campaign) {}
    DeviceMenu(CampaignState &campaign, SOLARSYS_STATE &system);
    BOOLEAN run(); // Legacy result: whether the parent menu may continue.
private:
    bool useCaster();
    DeviceStatus invoke(BYTE device);
    static BOOLEAN manipulate(MENU_STATE *menu);
    bool inOrbit() const noexcept { return system_ && system_->InOrbit; }
    CampaignState &campaign_;
    SOLARSYS_STATE *system_ = nullptr; // Optional borrowed exploration descriptor.
};
} // namespace uqm
#endif
