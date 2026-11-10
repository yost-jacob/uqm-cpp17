#ifndef UQM_GAME_CONFIGURATION_HPP_
#define UQM_GAME_CONFIGURATION_HPP_
#include "game_configuration.h"
#include <memory>
namespace uqm {
using GameConfiguration = UQM_GAME_CONFIGURATION;
// Main's C startup fills the record before UQM_Context exists. Transfer its
// unique ownership to the context without moving data or changing C pointers.
// Return ownership for main's final backend teardown after context destruction.
class ConfigurationOwner {
public:
    ConfigurationOwner();
    ~ConfigurationOwner();
    ConfigurationOwner(const ConfigurationOwner &) = delete;
    ConfigurationOwner &operator=(const ConfigurationOwner &) = delete;
    GameConfiguration &get() noexcept { return *value_; }
    const GameConfiguration &get() const noexcept { return *value_; }
private:
    std::unique_ptr<GameConfiguration> value_;
};
}
#endif
