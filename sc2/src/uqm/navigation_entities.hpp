#ifndef UQM_NAVIGATION_ENTITIES_HPP_
#define UQM_NAVIGATION_ENTITIES_HPP_
#include "hyper_navigation.hpp"
#include "combat_entity_legacy.hpp"
#include "ship.h"
namespace uqm::detail {
// Engine slots own these behaviors. Navigation outlives every slot and resets
// the engine before its resource lease ends. Hooks borrow ELEMENTs only for the
// current pass; expiration/collision keep the legacy simulation phase ordering.
class NavigationFlagship final : public uqm::combat::CombatEntity {
public:
    enum class Phase { Flying, Arriving, Departing };
    explicit NavigationFlagship(uqm::SpaceNavigation &navigation) : navigation(navigation) {}
    void update(ELEMENT &self) override {
        switch (phase_) {
            case Phase::Flying: ship_preprocess(&self); break;
            case Phase::Arriving: navigation.hyper_transition(&self); break;
            case Phase::Departing: navigation.unhyper_transition(&self); break;
        }
    }
    void postprocess(ELEMENT &self) override { ship_postprocess(&self); }
    void phase(ELEMENT &self, Phase value) {
        phase_ = value;
        uqm::detail::enableEntityHooks(self, EntityHooks::Update);
        if (value == Phase::Flying)
            uqm::detail::enableEntityHooks(self, EntityHooks::Postprocess);
        else self.postprocess_func = nullptr;
    }
    uqm::SpaceNavigation &navigation;
private:
    Phase phase_ = Phase::Flying;
};
class NavigationStar final : public uqm::combat::CombatEntity {
public:
    explicit NavigationStar(uqm::SpaceNavigation &navigation) : navigation_(navigation) {}
    void update(ELEMENT &) override {}
    void expire(ELEMENT &self) override { navigation_.hyper_death(&self); }
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override {
        navigation_.hyper_collision(&self, &point, &other, &otherPoint);
    }
private:
    uqm::SpaceNavigation &navigation_;
};
class NavigationPortal final : public uqm::combat::CombatEntity {
public:
    explicit NavigationPortal(uqm::SpaceNavigation &navigation) : navigation_(navigation) {}
    void update(ELEMENT &) override {}
    void expire(ELEMENT &self) override { navigation_.arilou_space_death(&self); }
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override {
        navigation_.arilou_space_collision(&self, &point, &other, &otherPoint);
    }
private:
    uqm::SpaceNavigation &navigation_;
};
class NavigationEncounter final : public uqm::combat::CombatEntity {
public:
    explicit NavigationEncounter(uqm::SpaceNavigation &navigation) : navigation_(navigation) {}
    void update(ELEMENT &) override {}
    void expire(ELEMENT &self) override { navigation_.encounter_transition(&self); }
    void collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) override {
        navigation_.encounter_collision(&self, &point, &other, &otherPoint);
    }
private:
    uqm::SpaceNavigation &navigation_;
};
} // namespace uqm::detail
#endif
