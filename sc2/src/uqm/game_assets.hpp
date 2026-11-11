#ifndef UQM_GAME_ASSETS_HPP_
#define UQM_GAME_ASSETS_HPP_
#include "asset_cache.hpp"
#include "libs/gfxlib.h"
#include "libs/sndlib.h"
namespace uqm {
// Own captured backend resources. Handles are borrowed views, valid as long as
// the shared asset is retained. Callers own animation/playback state separately.
// resource must be a non-null resource-index ID. Missing data throws runtime_error;
// construction and final release must obey the backend resource-thread contract.
class GraphicAsset final : public GameAsset {
public:
    explicit GraphicAsset(RESOURCE resource);
    ~GraphicAsset() override;
    FRAME frame() const noexcept { return frame_; }
private: FRAME frame_;
};
class MusicAsset final : public GameAsset {
public:
    explicit MusicAsset(RESOURCE resource);
    ~MusicAsset() override;
    MUSIC_REF music() const noexcept { return music_; }
private: MUSIC_REF music_;
};
class SoundAsset final : public GameAsset {
public:
    explicit SoundAsset(RESOURCE resource);
    ~SoundAsset() override;
    SOUND sound() const noexcept { return sound_; }
private: SOUND sound_;
};
}
#endif
