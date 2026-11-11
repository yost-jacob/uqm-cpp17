#include "game_assets.hpp"
#include <stdexcept>
namespace uqm {
GraphicAsset::GraphicAsset(RESOURCE resource) : frame_(CaptureDrawable(static_cast<DRAWABLE>(LoadGraphicInstance(resource)))) {
    if (!frame_) throw std::runtime_error("could not load graphic: " + std::string(resource));
}
MusicAsset::MusicAsset(RESOURCE resource) : music_(LoadMusicInstance(resource)) {
    if (!music_) throw std::runtime_error("could not load music: " + std::string(resource));
}
SoundAsset::SoundAsset(RESOURCE resource) : sound_(CaptureSound(LoadSoundInstance(resource))) {
    if (!sound_) throw std::runtime_error("could not load sound: " + std::string(resource));
}
GraphicAsset::~GraphicAsset() { if (frame_) DestroyDrawable(ReleaseDrawable(frame_)); }
MusicAsset::~MusicAsset() { if (music_) DestroyMusic(music_); }
SoundAsset::~SoundAsset() { if (sound_) DestroySound(ReleaseSound(sound_)); }
namespace {
std::unique_ptr<GameAsset> loadAsset(const AssetId &id) {
    // make_unique allocates the owner before its constructor loads the handle.
    switch (id.kind) {
        case AssetKind::Graphic: return std::make_unique<GraphicAsset>(id.resource.c_str());
        case AssetKind::Music: return std::make_unique<MusicAsset>(id.resource.c_str());
        case AssetKind::Sound: return std::make_unique<SoundAsset>(id.resource.c_str());
    }
    throw std::invalid_argument("unknown asset kind");
}
}
AssetCache::AssetCache() : AssetCache(loadAsset) {}
}
