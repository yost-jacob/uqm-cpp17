# Session asset ownership

GameSession and ArcadeSession each own an AssetCache. It is their first member,
so it is destroyed after their modes, simulation queues and other asset owners.
Sessions do not share their caches. The existing UQM_SYSTEM_RUNTIME remains an
exploration visit; no application-wide services owner is required for this cache.

AssetCache::find takes an AssetId (resource kind and owned resource-index string)
and returns shared_ptr<const GameAsset>. It loads missing assets automatically.
The caller retains that pointer for as long as its borrowed backend handles are
needed. GraphicAsset, MusicAsset and SoundAsset own the loaded backend resources;
they release them on destruction. Asset configuration, animation progress,
playback state and gameplay quantities are not stored in the cache.

## Synchronization and reclamation

The container and mutexes are private. Methods use RAII lock guards. A hit locks
the map briefly and promotes the stored weak_ptr to shared ownership. A miss
acquires the separate loading mutex, checks the map again, loads outside the map
lock, then publishes under the map lock. Concurrent misses therefore do not load
the same still-live asset twice. Loader exceptions and null results leave the
request retryable. Custom loaders must not re-enter the same cache.

The cache stores only weak_ptrs. The last shared owner's custom deleter destroys
the asset outside the map lock, then calls the cache's private erase method with
its resource ID and generation token. erase checks that token under its own lock.
If a replacement was loaded while the old asset was being destroyed, its newer
entry survives. There is no periodic pruning pass or use_count-based lifetime
logic. Key capture and shared-owner construction preserve rollback if allocation
fails, and map-insertion failure releases the map lock before destroying assets.

All asset owners and worker threads must finish before the session/cache is
destroyed. The deleter borrows that stable cache; it does not extend the session's
lifetime. Backend services outlive sessions. Cache synchronization protects its
index, not SDL or resource-backend thread affinity. Production loads and releases
continue on the existing game/resource thread; this change starts no background
loading threads.

## First production consumer

Native combat Ship objects retain a vector of shared asset owners. The C-facing
RACE_DESC contains borrowed graphics/music/sound handles, while mutable ship and
captain-animation fields remain unique per ship. Missing smaller animation views
alias the last available view, preserving the old loading rule. Switching a
borrowed frame index does not transfer ownership of its drawable.

The load_ship C boundary resolves the selected campaign or arcade cache once and
passes it into native loading. All battle sprites, captain backgrounds, victory
music and sound samples use it. Sa-Matra's one-shot captain backdrop now drops
its descriptor view without manually destroying the shared resource.

Battle asset destruction follows Ship lifetime. The obsolete FreeBattleData
argument was removed from free_ship and its C callers. The remaining FreeIconData
argument preserves transfer of uncached icons and race strings into legacy
fleet/menu storage. Partial icon loading still has a rollback scope; partial
battle loading is cleaned up automatically by the Ship's shared owners.

Fleet/menu icons, dialogue resources, planetary generation output, shared system
artwork and application/device resources have not all migrated to this cache.
World-owned generated planet art remains mutable per body and is not deduplicated
as an immutable asset. Further consumers can migrate incrementally without
changing the session ownership or cache contract.
