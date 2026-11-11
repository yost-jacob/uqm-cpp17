#include "uqm/game_mode.hpp"
#include "uqm/game_state.hpp"
#include "uqm/simulation_engine_legacy.hpp"
#include <boost/test/unit_test.hpp>
#include <stdexcept>
using namespace uqm;
namespace {
ELEMENT &addEntity(SimulationEngine &engine) {
    auto &queue = detail::SimulationAccess::queue(engine);
    auto handle = AllocLink(&queue);
    auto &element = *static_cast<ELEMENT *>(handle);
    element = {}; PutQueue(&queue, handle); return element;
}
struct Trace { unsigned initialized = 0; bool childDestroyed = false, parentDestroyed = false, resumed = false; };
class Child final : public GameMode {
public:
    Child(ModeDispatcher &host, Trace &trace, PRIMITIVE *parentPrimitives)
        : GameMode(host), trace_(trace), parentPrimitives_(parentPrimitives) {}
    ~Child() override {
        trace_.childDestroyed = &simulationEngine() == &engine();
    }
    ModeResult run() override {
        BOOST_TEST(engine().size() == 1u);
        DisplayFreeList = 3;
        SetPrimType(&DisplayArray[0], LINE_PRIM);
        return ModeCompleted{};
    }
protected:
    void initialize() override {
        ++trace_.initialized;
        BOOST_TEST(&simulationEngine() == &engine());
        BOOST_TEST(engine().size() == 0u);
        BOOST_TEST(DisplayArray != parentPrimitives_);
        addEntity(engine());
    }
private:
    Trace &trace_; PRIMITIVE *parentPrimitives_;
};
class Parent final : public GameMode {
public:
    Parent(GameSession &session, Trace &trace)
        : GameMode(session.dispatcher(), session.nextModeSeed()), session_(session), trace_(trace) {}
    ~Parent() override { trace_.parentDestroyed = &simulationEngine() == &engine(); }
    ModeResult run() override {
        auto &entity = addEntity(engine());
        const auto id = combat::EntityHandle::capture(detail::SimulationAccess::queue(engine()), entity);
        auto *primitives = DisplayArray;
        DisplayFreeList = 11;
        SetPrimType(&DisplayArray[0], STAMP_PRIM);
        BOOST_TEST(!session_.swapGameMode(std::make_unique<Child>(dispatcher(), trace_, primitives)));
        BOOST_REQUIRE(engine().resolve(id));
        BOOST_TEST(&engine().resolve(id)->get() == &entity);
        BOOST_TEST(DisplayArray == primitives);
        BOOST_TEST(DisplayFreeList == 11u);
        BOOST_TEST(GetPrimType(&DisplayArray[0]) == STAMP_PRIM);
        return SessionExit::title();
    }
protected:
    void initialize() override { ++trace_.initialized; }
    void resume() override {
        trace_.resumed = true;
        BOOST_TEST(trace_.childDestroyed);
        BOOST_TEST(&simulationEngine() == &engine());
    }
private:
    GameSession &session_; Trace &trace_;
};
}
BOOST_AUTO_TEST_CASE(mode_initialization_and_child_teardown_preserve_parent_scene)
{
    auto state = std::make_shared<GameState>(UserProfile{SinglePlayerId, "Scenes"});
    GameSession session(state, SinglePlayerId); Trace trace;
    auto *previous = &simulationEngine();
    const auto result = session.run(std::make_unique<Parent>(session, trace));
    BOOST_TEST(static_cast<int>(result.kind) == static_cast<int>(SessionExitKind::ReturnToTitle));
    BOOST_TEST(trace.initialized == 2u);
    BOOST_TEST(trace.childDestroyed); BOOST_TEST(trace.parentDestroyed); BOOST_TEST(trace.resumed);
    BOOST_TEST(&simulationEngine() == previous);
}
BOOST_AUTO_TEST_CASE(scene_transfer_returns_updated_entities_and_projection_without_copying_handles)
{
    SimulationEngine parent(3), child(2);
    SimulationBinding parentBinding(parent);
    auto &entity = addEntity(parent); entity.hit_points = 5;
    const auto id = combat::EntityHandle::capture(detail::SimulationAccess::queue(parent), entity);
    SetPrimType(&DisplayArray[0], STAMP_PRIM); DisplayFreeList = 7;
    {
        SimulationBinding childBinding(child);
        SimulationSceneTransfer scene(parent, child);
        BOOST_TEST(!parent.resolve(id)); BOOST_REQUIRE(child.resolve(id));
        child.resolve(id)->get().hit_points = 2;
        DisplayFreeList = 9; SetPrimType(&DisplayArray[0], LINE_PRIM);
    }
    BOOST_REQUIRE(parent.resolve(id)); BOOST_TEST(!child.resolve(id));
    BOOST_TEST(&parent.resolve(id)->get() == &entity);
    BOOST_TEST(parent.resolve(id)->get().hit_points == 2u);
    BOOST_TEST(DisplayFreeList == 9u);
    BOOST_TEST(GetPrimType(&DisplayArray[0]) == LINE_PRIM);
}
BOOST_AUTO_TEST_CASE(failed_mode_initialization_restores_previous_engine_binding)
{
    auto state = std::make_shared<GameState>(UserProfile{SinglePlayerId, "Failure"});
    GameSession session(state, SinglePlayerId); bool destroyed = false;
    auto *previous = &simulationEngine();
    struct Failed final : GameMode {
        bool &destroyed;
        Failed(ModeDispatcher &host, bool &value)
            : GameMode(host), destroyed(value) {}
        ~Failed() override { destroyed = &simulationEngine() == &engine(); }
        void initialize() override { throw std::runtime_error("cannot initialize scene"); }
        ModeResult run() override { BOOST_FAIL("run after failed initialization"); return ModeCompleted{}; }
    };
    const auto result = session.run(std::make_unique<Failed>(session.dispatcher(), destroyed));
    BOOST_TEST(static_cast<int>(result.kind) == static_cast<int>(SessionExitKind::Failed));
    BOOST_TEST(destroyed); BOOST_TEST(&simulationEngine() == previous);
}
