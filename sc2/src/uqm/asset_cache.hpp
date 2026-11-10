#ifndef UQM_ASSET_CACHE_HPP_
#define UQM_ASSET_CACHE_HPP_
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace uqm {
enum class AssetKind { Graphic, Music, Sound };
struct AssetId {
    AssetKind kind;
    std::string resource;
    bool operator==(const AssetId &other) const noexcept {
        return kind == other.kind && resource == other.resource;
    }
};
// Immutable shared resource; concrete resource owners perform backend cleanup.
class GameAsset {
public:
    GameAsset() = default;
    virtual ~GameAsset() = default;
    GameAsset(const GameAsset &) = delete;
    GameAsset &operator=(const GameAsset &) = delete;
};
class AssetCache {
public:
    using Pointer = std::shared_ptr<const GameAsset>;
    using Loader = std::function<std::unique_ptr<GameAsset>(const AssetId &)>;
    AssetCache(); // Uses the existing graphic/music/sound resource backend.
    // loader must return an owned, non-null asset or throw; it must not re-enter
    // this cache. Backend thread requirements still apply to load/destruction.
    explicit AssetCache(Loader loader);
    AssetCache(const AssetCache &) = delete;
    AssetCache &operator=(const AssetCache &) = delete;
    // Internally synchronized. Loads on demand; failures propagate and can be
    // retried. Owners must release their pointers before this cache is destroyed.
    Pointer find(const AssetId &id);
    std::size_t size() const; // Number of indexed assets, not a retention policy.
private:
    struct Hash { std::size_t operator()(const AssetId &id) const noexcept; };
    using Token = std::uint64_t;
    struct Entry { std::weak_ptr<const GameAsset> asset; Token token; };
    // Called by the last owner's deleter. A retiring asset cannot erase a newer
    // replacement for the same ID. Never destroys an asset while holding mutex_.
    void erase(const AssetId &id, Token token) noexcept;
    Loader loader_;
    mutable std::mutex mutex_;
    std::mutex loadingMutex_; // Serialize misses without holding the map lock.
    std::unordered_map<AssetId, Entry, Hash> entries_;
    Token nextToken_ = 0; // Protected by loadingMutex_.
};
}
#endif
