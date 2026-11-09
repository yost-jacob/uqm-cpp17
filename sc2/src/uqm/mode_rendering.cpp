#include "mode_rendering.hpp"
#include "controls.h"
#include "libs/graphics/context.h"
#include "libs/graphics/gfx_common.h"
#include <vector>
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
class DrawingBindings final : public ModeSuspension {
public:
    DrawingBindings(std::initializer_list<CONTEXT> contexts)
        : active_(GetContext()), contexts_{},
          scale_(GetGraphicScale()), scaleMode_(GetGraphicScaleMode()),
          callback_(nullptr) {
        contexts_.reserve(contexts.size());
        for (auto context : contexts) contexts_.emplace_back(context);
        callback_ = SetInputCallback(nullptr);
    }
    ~DrawingBindings() override {
        for (auto &context : contexts_) context.restore();
        SetGraphicScale(scale_);
        SetGraphicScaleMode(scaleMode_);
        SetContext(active_);
        SetInputCallback(callback_);
    }
private:
    CONTEXT active_;
    std::vector<ContextBindings> contexts_;
    int scale_, scaleMode_;
    InputFrameCallback *callback_;
};
}

std::unique_ptr<ModeSuspension> suspendDrawingBindings(std::initializer_list<CONTEXT> contexts) {
    return std::make_unique<DrawingBindings>(contexts);
}
}
