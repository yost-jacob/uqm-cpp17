#ifndef UQM_PLANET_SPHERE_HPP_
#define UQM_PLANET_SPHERE_HPP_
#include "planets/planets.h"
#include "libs/timelib.h"
#include <array>
#include <vector>
// Mutable projection and rotation work for one generated surface. Allocated
// lazily by PlanetRenderResources; the C graphics record only borrows it.
struct PlanetSphereData {
    static constexpr int radius = 37;
    static constexpr int diameter = radius * 2 + 1;
    struct MapPoint { POINT p[4]{}; DWORD m[4]{}; };
    std::array<std::array<DWORD, diameter>, diameter> lighting{};
    std::array<std::array<MapPoint, diameter>, diameter> projection{};
    std::vector<SBYTE> elevation;
    std::vector<Color> colors, scratch;
    int frame = 0, direction = 0, point = 0;
    bool throb = false, shielded = false;
    TimeCount nextFrame = 0;
    // Advance one map step for direction +/-1 and positive mapWidth. Preparing
    // a frame never changes the caller's scheduling deadline.
    void advance(int mapWidth) noexcept {
        frame ^= 1;
        point += direction;
        if (point < 0) point = mapWidth - 1;
        else if (point >= mapWidth) point = 0;
    }
};
namespace uqm {
// All graphics/sphere borrows last only for the call. An initialized surface is
// required for rendering/advancement; redraw alone does not advance animation.
void DrawPlanet(PLANET_RENDER_DATA &graphics, int tintY, Color tint);
void DrawPlanetSphere(PLANET_RENDER_DATA &graphics, int x, int y);
void DrawDefaultPlanetSphere(PLANET_RENDER_DATA &graphics);
void InitSphereRotation(PLANET_RENDER_DATA &graphics, int direction, bool shielded);
void UninitSphereRotation(PLANET_ORBIT &orbit);
void PrepareNextRotationFrame(PLANET_RENDER_DATA &graphics);
void RenderPlanetSphere(PLANET_RENDER_DATA &graphics, FRAME frame, int offset, bool throb);
void SetShieldThrobEffect(PLANET_RENDER_DATA &graphics, FRAME from, int offset, FRAME to);
void ZoomInPlanetSphere(PLANET_RENDER_DATA &graphics);
void RotatePlanetSphere(PLANET_RENDER_DATA &graphics, bool keepRate);
// Body, analysis and star-relative position are explicit generation inputs.
// Optional surface-definition art is consumed by the generated surface.
void GeneratePlanetSurface(PLANET_RENDER_DATA &graphics, const PLANET_GENERATED_DATA &planet,
    const PLANET_DESC &body, POINT starRelativePosition, FRAME definition, RandomContext &random);
}
#endif
