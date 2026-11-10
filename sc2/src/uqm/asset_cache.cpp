#include "asset_cache.hpp"
#include <stdexcept>
#include <utility>
namespace uqm {
AssetCache::AssetCache(Loader loader) : loader_(std::move(loader)) {
    if (!loader_) throw std::invalid_argument("asset cache requires a loader");
}
std::size_t AssetCache::Hash::operator()(const AssetId &id) const noexcept {
    return std::hash<std::string>{}(id.resource) ^ (static_cast<std::size_t>(id.kind) << 1);
}
AssetCache::Pointer AssetCache::find(const AssetId &id) {
    if (id.resource.empty()) throw std::invalid_argument("asset requires a resource ID");
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = entries_.find(id);
        if (found != entries_.end())
            if (auto asset = found->second.asset.lock()) return asset;
    }
    // Load outside the map lock: partial-load rollback and custom deleters may
    // erase other entries. A second lookup prevents duplicate concurrent loads.
    std::lock_guard<std::mutex> loading(loadingMutex_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto found = entries_.find(id);
        if (found != entries_.end())
            if (auto asset = found->second.asset.lock()) return asset;
    }
    auto loaded = loader_(id);
    if (!loaded) throw std::runtime_error("asset loader returned null: " + id.resource);
    const auto token = ++nextToken_;
    auto deleter = [this, id, token](const GameAsset *value) noexcept {
        delete value;
        erase(id, token);
    };
    // Capture the key before releasing unique ownership: copying it may throw.
    auto asset = Pointer(loaded.release(), std::move(deleter));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.insert_or_assign(id, Entry{asset, token});
    }
    return asset;
}
void AssetCache::erase(const AssetId &id, Token token) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto found = entries_.find(id);
    if (found != entries_.end() && found->second.token == token) entries_.erase(found);
}
std::size_t AssetCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}
}
