#include "ship_tuning.hpp"
#include "ship_tuning_c.h"
#include "libs/reslib.h"
#include "libs/log.h"
#include "options.h"
#include <memory>
#include <sstream>
#include <stdexcept>

namespace {
std::unique_ptr<const uqm::combat::ShipTuningCatalog> catalog;
}
namespace uqm::combat {
const ShipTuning &shipTuning(std::string_view id) {
    if (!catalog) throw std::logic_error("ship tuning must be loaded before ship creation");
    return catalog->at(id);
}
}
extern "C" BOOLEAN LoadShipTuning(void) {
    if (catalog) return TRUE;
    try {
        using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;
        const Stream file(res_OpenResFile(contentDir, "base/ships.json", "rb"), res_CloseResFile);
        if (!file) throw std::runtime_error("cannot open ship tuning");
        std::string bytes(LengthResFile(file.get()), '\0');
        if (ReadResFile(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
            throw std::runtime_error("could not read complete ship tuning");
        std::istringstream input(bytes);
        catalog = std::make_unique<const uqm::combat::ShipTuningCatalog>(input);
        return TRUE;
    } catch (const std::exception &error) {
        log_add(log_Error, "base/ships.json: %s", error.what());
        return FALSE;
    } catch (...) {
        log_add(log_Error, "base/ships.json: unknown loading failure");
        return FALSE;
    }
}
