#include "game_configuration.hpp"
#include <mutex>
#include <stdexcept>
namespace {
std::unique_ptr<uqm::GameConfiguration> &bootstrap() {
    static auto value = std::make_unique<uqm::GameConfiguration>();
    return value;
}
uqm::GameConfiguration *record() {
    static auto *const address = bootstrap().get();
    return address;
}
std::mutex &ownershipMutex() { static std::mutex value; return value; }
}
extern "C" UQM_GAME_CONFIGURATION *GetGameConfiguration(void) { return record(); }
namespace uqm {
ConfigurationOwner::ConfigurationOwner() {
    std::lock_guard<std::mutex> lock{ownershipMutex()};
    record();
    if (!bootstrap()) throw std::logic_error("application configuration already has a context owner");
    value_ = std::move(bootstrap());
}
ConfigurationOwner::~ConfigurationOwner() {
    std::lock_guard<std::mutex> lock{ownershipMutex()};
    bootstrap() = std::move(value_);
}
}
