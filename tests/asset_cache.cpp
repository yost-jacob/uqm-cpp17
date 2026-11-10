#include "uqm/asset_cache.hpp"
#include "uqm/game_assets.hpp"
#include "uqm/game_state.hpp"
#include "uqm/arcade_session.hpp"
#include <boost/test/unit_test.hpp>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <vector>
using namespace uqm;
namespace {
struct TestAsset final : GameAsset {
    TestAsset(std::atomic<unsigned> &destroyed, std::function<void()> retiring = {})
        : destroyed(destroyed), retiring(std::move(retiring)) {}
    ~TestAsset() override { if (retiring) retiring(); ++destroyed; }
    std::atomic<unsigned> &destroyed;
    std::function<void()> retiring;
};
const AssetId graphic{AssetKind::Graphic, "test.cached"};
}
BOOST_AUTO_TEST_CASE(asset_cache_loads_lazily_shares_and_reclaims_its_index)
{
    std::atomic<unsigned> destroyed{0}, loads{0};
    AssetCache cache([&](const AssetId &) { ++loads; return std::make_unique<TestAsset>(destroyed); });
    BOOST_TEST(cache.size() == 0u);
    BOOST_TEST(loads == 0u);
    auto first = cache.find(graphic);
    auto second = cache.find(graphic);
    BOOST_TEST(first.get() == second.get());
    BOOST_TEST(loads == 1u);
    first.reset();
    BOOST_TEST(destroyed == 0u);
    second.reset();
    BOOST_TEST(destroyed == 1u);
    BOOST_TEST(cache.size() == 0u);
    auto reloaded = cache.find(graphic);
    BOOST_TEST(loads == 2u);
    reloaded.reset();
    BOOST_TEST(cache.size() == 0u);
    BOOST_TEST(destroyed == 2u);
}
BOOST_AUTO_TEST_CASE(concurrent_asset_requests_publish_one_loaded_owner)
{
    std::atomic<unsigned> destroyed{0}, loads{0};
    AssetCache cache([&](const AssetId &) { ++loads; return std::make_unique<TestAsset>(destroyed); });
    constexpr unsigned count = 12;
    std::mutex gate;
    std::condition_variable changed;
    unsigned ready = 0;
    bool start = false;
    std::vector<AssetCache::Pointer> assets(count);
    std::vector<std::exception_ptr> errors(count);
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < count; ++i) workers.emplace_back([&, i] {
        {
            std::unique_lock<std::mutex> lock(gate);
            ++ready; changed.notify_all();
            changed.wait(lock, [&] { return start; });
        }
        try { assets[i] = cache.find(graphic); }
        catch (...) { errors[i] = std::current_exception(); }
    });
    {
        std::unique_lock<std::mutex> lock(gate);
        changed.wait(lock, [&] { return ready == count; });
        start = true;
    }
    changed.notify_all();
    for (auto &worker : workers) worker.join();
    for (unsigned i = 0; i < count; ++i) {
        BOOST_TEST(!errors[i]);
        BOOST_TEST(assets[i].get() == assets.front().get());
    }
    BOOST_TEST(loads == 1u);
    assets.clear();
    BOOST_TEST(destroyed == 1u);
    BOOST_TEST(cache.size() == 0u);
}
BOOST_AUTO_TEST_CASE(retiring_asset_cannot_erase_a_concurrently_loaded_replacement)
{
    std::atomic<unsigned> destroyed{0}, loads{0};
    std::mutex gate;
    std::condition_variable changed;
    bool retiring = false, finish = false;
    AssetCache cache([&](const AssetId &) -> std::unique_ptr<GameAsset> {
        if (++loads != 1) return std::make_unique<TestAsset>(destroyed);
        return std::make_unique<TestAsset>(destroyed, [&] {
            std::unique_lock<std::mutex> lock(gate);
            retiring = true; changed.notify_all();
            changed.wait(lock, [&] { return finish; });
        });
    });
    auto first = cache.find(graphic);
    std::thread release([asset = std::move(first)]() mutable { asset.reset(); });
    {
        std::unique_lock<std::mutex> lock(gate);
        changed.wait(lock, [&] { return retiring; });
    }
    auto replacement = cache.find(graphic); // The old weak_ptr has expired.
    {
        std::lock_guard<std::mutex> lock(gate);
        finish = true;
    }
    changed.notify_all();
    release.join();
    BOOST_TEST(cache.size() == 1u);
    BOOST_TEST(cache.find(graphic).get() == replacement.get());
    BOOST_TEST(loads == 2u);
    replacement.reset();
    BOOST_TEST(cache.size() == 0u);
    BOOST_TEST(destroyed == 2u);
}
BOOST_AUTO_TEST_CASE(asset_failures_are_retryable_and_kinds_have_distinct_keys)
{
    std::atomic<unsigned> destroyed{0};
    unsigned attempts = 0;
    AssetCache cache([&](const AssetId &) -> std::unique_ptr<GameAsset> {
        if (++attempts == 1) throw std::runtime_error("missing file");
        if (attempts == 2) return {};
        return std::make_unique<TestAsset>(destroyed);
    });
    BOOST_CHECK_THROW(cache.find(graphic), std::runtime_error);
    BOOST_TEST(cache.size() == 0u);
    BOOST_CHECK_THROW(cache.find(graphic), std::runtime_error);
    BOOST_TEST(cache.size() == 0u);
    auto image = cache.find(graphic);
    auto sound = cache.find({AssetKind::Sound, graphic.resource});
    BOOST_TEST(image.get() != sound.get());
    BOOST_TEST(cache.size() == 2u);
    BOOST_CHECK_THROW(cache.find({AssetKind::Graphic, ""}), std::invalid_argument);
}
BOOST_AUTO_TEST_CASE(campaign_and_arcade_instances_own_separate_asset_caches)
{
    auto state = std::make_shared<GameState>(UserProfile{SinglePlayerId, "Player"});
    GameSession campaign{state, SinglePlayerId};
    ArcadeSession arcade;
    auto campaignAsset = campaign.assets().find(graphic);
    auto arcadeAsset = arcade.assets().find(graphic);
    BOOST_TEST(campaignAsset.get() != arcadeAsset.get());
    BOOST_TEST(campaign.assets().size() == 1u);
    BOOST_TEST(arcade.assets().size() == 1u);
    campaignAsset.reset();
    BOOST_TEST(campaign.assets().size() == 0u);
    BOOST_TEST(arcade.assets().size() == 1u);
}
