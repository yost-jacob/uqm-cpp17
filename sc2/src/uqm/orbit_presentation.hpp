#ifndef UQM_ORBIT_PRESENTATION_HPP_
#define UQM_ORBIT_PRESENTATION_HPP_
struct solarsys_state;
namespace uqm {
// Visit-owned display/audio lifetime. The system's graphics borrow outlives
// this scope. Cleanup also runs if terrain or node generation fails halfway.
class OrbitPresentation {
public:
    explicit OrbitPresentation(solarsys_state &system);
    ~OrbitPresentation();
    OrbitPresentation(const OrbitPresentation &) = delete;
    OrbitPresentation &operator=(const OrbitPresentation &) = delete;
private:
    solarsys_state &system_;
};
}
#endif
