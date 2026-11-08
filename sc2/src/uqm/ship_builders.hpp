#ifndef UQM_SHIP_BUILDERS_HPP_
#define UQM_SHIP_BUILDERS_HPP_
#include <memory>
namespace uqm::combat {
class Ship;
// Internal builders implemented beside their concrete classes. Native ownership
// remains unique throughout construction; only load_ship crosses the C boundary.
std::unique_ptr<Ship> makeArilouShip();
std::unique_ptr<Ship> makeChmmrShip();
std::unique_ptr<Ship> makeEarthlingShip();
std::unique_ptr<Ship> makeOrzShip();
std::unique_ptr<Ship> makePkunkShip();
std::unique_ptr<Ship> makeShofixtiShip();
std::unique_ptr<Ship> makeSpathiShip();
std::unique_ptr<Ship> makeSupoxShip();
std::unique_ptr<Ship> makeThraddashShip();
std::unique_ptr<Ship> makeUtwigShip();
std::unique_ptr<Ship> makeVuxShip();
std::unique_ptr<Ship> makeYehatShip();
std::unique_ptr<Ship> makeMelnormeShip();
std::unique_ptr<Ship> makeDruugeShip();
std::unique_ptr<Ship> makeIlwrathShip();
std::unique_ptr<Ship> makeMyconShip();
std::unique_ptr<Ship> makeSlylandroShip();
std::unique_ptr<Ship> makeUmgahShip();
std::unique_ptr<Ship> makeUrQuanShip();
std::unique_ptr<Ship> makeZoqFotPikShip();
std::unique_ptr<Ship> makeSyreenShip();
std::unique_ptr<Ship> makeKohrAhShip();
std::unique_ptr<Ship> makeAndrosynthShip();
std::unique_ptr<Ship> makeChenjesuShip();
std::unique_ptr<Ship> makeMmrnmhrmShip();
std::unique_ptr<Ship> makeFlagship();
std::unique_ptr<Ship> makeSaMatraShip();
std::unique_ptr<Ship> makeUrQuanProbeShip();
}
#endif
