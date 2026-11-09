#include "system_runtime.hpp"
#include "exploration_rendering.hpp"
#include "game_mode.hpp"
#include "controls.h"
#include "planets/scan.h"
#include "setup.h"
#include "libs/graphics/context.h"
#include "libs/graphics/gfx_common.h"
#include <array>

namespace uqm {
namespace {
// Only the backend knows context_desc. Borrow parent-owned handles; never copy
// renderer caches/backing pointers or delete resources belonging to the parent.
struct ContextBindings {
    CONTEXT context = nullptr;
    FRAME frame = nullptr, effect = nullptr;
    FONT font = nullptr;
    Color foreground{}, background{};
    DrawMode mode{};
    RECT clip{};
    POINT origin{};
    explicit ContextBindings(CONTEXT value) : context(value) {
        if (!context) return;
        frame = context->ForeGroundFrame;
        effect = context->FontEffect;
        font = context->Font;
        foreground = context->ForeGroundColor;
        background = context->BackGroundColor;
        mode = context->Mode;
        clip = context->ClipRect;
        if (frame) origin = GetFrameHot(frame);
    }
    void restore() noexcept {
        if (!context) return;
        SetContext(context);
        SetContextFGFrame(frame);
        SetContextFont(font);
        SetContextFontEffect(effect);
        SetContextForeGroundColor(foreground);
        SetContextBackGroundColor(background);
        SetContextDrawMode(mode);
        SetContextClipRect(clip.extent.width ? &clip : nullptr);
        if (frame) SetContextOrigin(origin);
    }
};
class ExplorationModeSuspension final : public ModeSuspension {
public:
    ExplorationModeSuspension()
        : active_(GetContext()), contexts_{{ContextBindings(PlanetContext), ContextBindings(SpaceContext),
                ContextBindings(RadarContext), ContextBindings(GetScanContext(nullptr))}},
          scale_(GetGraphicScale()), scaleMode_(GetGraphicScaleMode()),
          callback_(SetInputCallback(nullptr)) {}
    ~ExplorationModeSuspension() override {
        for (auto &context : contexts_) context.restore();
        SetGraphicScale(scale_);
        SetGraphicScaleMode(scaleMode_);
        SetContext(active_);
        SetInputCallback(callback_);
    }
private:
    CONTEXT active_;
    std::array<ContextBindings, 4> contexts_;
    int scale_, scaleMode_;
    InputFrameCallback *callback_;
};
}
std::unique_ptr<ModeSuspension> suspendExplorationRendering(ExplorationView view) {
    // Flight-time story encounters have no established orbital view yet.
    if (view == ExplorationView::Flight) return {};
    return std::make_unique<ExplorationModeSuspension>();
}
std::unique_ptr<ModeSuspension> SolarSystemExploration::suspendRendering() {
    return suspendExplorationRendering(runtime_.view);
}
void SolarSystemExploration::resumeRendering() {
    // Legacy engine exits may precede propagation to the session mailbox.
    if (GLOBAL(CurrentActivity) & (CHECK_ABORT | CHECK_LOAD | START_ENCOUNTER)) return;
    redrawExploration(runtime_.engine, runtime_.view);
}
}
