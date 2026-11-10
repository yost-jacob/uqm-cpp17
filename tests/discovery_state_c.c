#include "uqm/state.h"
#include "uqm/starmap.h"
#include "uqm/gendef.h"

/* Real planet-info adapters need only these runtime descriptors, not rendering.
 * The fixture supplies a layout; it never substitutes persistence functions. */
SOLARSYS_STATE *pSolarSysState;
int discovery_state_c_round_trip(void)
{
    SOLARSYS_STATE solar = {0};
    SOLARSYS_STATE *old_system = pSolarSysState;
    STAR_DESC *old_star = CurStarDescPtr;
    COUNT star, i;
    DWORD energy_offset;
    DWORD mask = 0;
    int ok = 0;
    GAME_STATE_FILE *file = NULL;
    PLANET_INFO_SNAPSHOT *snapshot = NULL;
    for (star = 0; star < NUM_SOLAR_SYSTEMS; ++star)
        if (GetStar(star)->Index == ANDROSYNTH_DEFINED) break;
    if (star == NUM_SOLAR_SYSTEMS) return 0;
    CurStarDescPtr = GetStar(star);
    pSolarSysState = &solar;
    solar.SunDesc[0].NumPlanets = 2;
    solar.PlanetDesc[0].NumPlanets = 2;
    solar.PlanetDesc[1].pPrevDesc = solar.SunDesc;
    solar.MoonDesc[0].pPrevDesc = &solar.PlanetDesc[1];
    InitPlanetInfo();
    file = OpenStateFile(STARINFO_FILE, "r+b");
    SeekStateFile(file, star * sizeof(DWORD), SEEK_SET);
    swrite_32(file, NUM_SOLAR_SYSTEMS * sizeof(DWORD));
    SeekStateFile(file, 0, SEEK_END);
    for (i = 0; i < 4 * NUM_SCAN_TYPES; ++i) swrite_32(file, 0);
    energy_offset = (NUM_SOLAR_SYSTEMS + 3 * NUM_SCAN_TYPES + ENERGY_SCAN) * sizeof(DWORD);
    SeekStateFile(file, energy_offset, SEEK_SET);
    swrite_32(file, UINT32_C(0x00050003));
    CloseStateFile(file);
    file = NULL;
    CreateSystemWorlds (&solar, CurStarDescPtr);
    BuildSystemWorlds (&solar, CurStarDescPtr);
    SelectSystemWorld (&solar, &solar.PlanetDesc[1]);
    ConfigurePersonalEnergyBehavior (&solar, 1, -1, 16, 4);
    BuildMoonWorlds (&solar, &solar.PlanetDesc[0]);
    ConfigureArtifactEnergyBehavior (&solar, 0, 1);
    {
        ENERGY_INTERACTION artifact = InteractWorldEnergy (&solar, 0, 1, 0, FALSE);
        ENERGY_INTERACTION collected = InteractWorldEnergy (&solar, 0, 1, 0, TRUE);
        if (!artifact.accepted || !artifact.removeNode || !artifact.hasReport
                || artifact.report != 0 || collected.accepted) goto done;
    }
    GetPlanetInfo();
    if (PersonalDiscoveryCount(star, 1, -1) != 2
            || solar.SysInfo.PlanetInfo.ScanRetrieveMask[ENERGY_SCAN] != 3) goto done;
    file = OpenStateFile(STARINFO_FILE, "rb");
    SeekStateFile(file, energy_offset, SEEK_SET);
    sread_32(file, &mask);
    CloseStateFile(file);
    file = NULL;
    if (mask != 3) goto done;
    {
        ENERGY_INTERACTION first = InteractWorldEnergy (&solar, 1, -1, 4, FALSE);
        ENERGY_INTERACTION repeat = InteractWorldEnergy (&solar, 1, -1, 4, FALSE);
        if (!first.accepted || first.removeNode || !first.hasReport || first.report != 2
                || repeat.accepted) goto done;
    }
    snapshot = CapturePlanetInfoForSave();
    if (!snapshot || PlanetInfoSnapshotData(snapshot)[energy_offset / sizeof(DWORD)] != UINT32_C(0x00150003)) goto done;
    /* Use the actual legacy archive load path: truncate, write, then orbit read. */
    file = OpenStateFile(STARINFO_FILE, "wb");
    if (PersonalDiscoveryCount(star, 1, -1) != 0) goto done;
    for (i = 0; i < PlanetInfoSnapshotLength(snapshot) / sizeof(DWORD); ++i)
        swrite_32(file, PlanetInfoSnapshotData(snapshot)[i]);
    CloseStateFile(file);
    file = NULL;
    ReleasePlanetInfoSnapshot(snapshot);
    snapshot = NULL;
    GetPlanetInfo();
    if (PersonalDiscoveryCount(star, 1, -1) != 3
            || solar.SysInfo.PlanetInfo.ScanRetrieveMask[ENERGY_SCAN] != 3) goto done;
    ok = 1;
done:
    if (file) CloseStateFile(file);
    ReleasePlanetInfoSnapshot(snapshot);
    DestroySystemWorlds (&solar);
    UninitPlanetInfo();
    pSolarSysState = old_system;
    CurStarDescPtr = old_star;
    return ok;
}
