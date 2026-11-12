#ifndef UQM_PLANET_REPORT_HPP_
#define UQM_PLANET_REPORT_HPP_
#include "planets/planets.h"
#include "planets/lander.h"
#include "nameref.h"
#include "resinst.h"
#include <utility>
namespace uqm {
// One orbital visit's discovery text and display resources. Children borrow this
// owner synchronously; destruction releases resources without publishing state.
class PlanetReport {
public:
    PlanetReport() = default;
    ~PlanetReport() {
        clear();
        if (font_) DestroyFont(font_);
        if (effect_) DestroyDrawable(ReleaseDrawable(effect_));
    }
    PlanetReport(const PlanetReport &) = delete;
    PlanetReport &operator=(const PlanetReport &) = delete;
    // Replace the report with the named resource. Text starts at entry zero;
    // fonts are loaded once per visit and retained across text replacement.
    void load(RESOURCE resource) {
        const auto text = CaptureStringTable(LoadStringTable(resource));
        clear();
        text_ = text;
        if (!font_) font_ = LoadFont(LANDER_FONT);
        if (!effect_) effect_ = CaptureDrawable(LoadGraphic(LANDER_FONTEFF_PMAP_ANIM));
    }
    void clear() noexcept {
        if (const auto text = std::exchange(text_, nullptr)) DestroyStringTable(ReleaseStringTable(text));
    }
    bool empty() const noexcept { return text_ == nullptr; }
    COUNT size() const { return text_ ? GetStringTableCount(text_) : 0; }
    // Absolute entry index in this table; out-of-range indices retain the
    // string library's wrapping behavior. An empty report stays empty.
    void select(COUNT index) { if (text_) text_ = SetAbsStringTableIndex(text_, index); }
    void advance() {
        if (!text_) return;
        text_ = SetRelStringTableIndex(text_, 1);
        if (GetStringTableIndex(text_) == 0) clear();
    }
    // Borrow display resources for this blocking report; no story effects.
    void show(SOUND menuSounds) const {
        if (text_) ShowDiscoveryReport(menuSounds, font_, effect_, text_);
    }
    // Lander callbacks enter with graphics batched. Restore that state even if
    // presentation throws; consume once, or advance until the table wraps.
    bool showLander(SOUND menuSounds, bool cycle) {
        if (empty()) return false;
        UnbatchGraphics();
        struct Rebatch { ~Rebatch() { BatchGraphics(); } } rebatch;
        show(menuSounds);
        if (cycle) advance(); else clear();
        return true;
    }
private:
    STRING text_ = nullptr;
    FONT font_ = nullptr;
    FRAME effect_ = nullptr;
};
}
#endif
