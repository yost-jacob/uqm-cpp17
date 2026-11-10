# Player location ownership

PlayerEntity owns one PlayerLocation variant: HyperSpaceLocation,
QuasiSpaceLocation, or SolarSystemLocation. It is session/player state, separate
from the mode's simulation engine and from generated World objects and graphics.

Modes use std::get for their expected alternative. A wrong alternative throws
std::bad_variant_access, which the dispatcher treats like other mode failures.
There are no repeated optional lookups to hide an incorrect dispatch. Only
realm entry decides whether to replace the alternative; constructing a successor
mode does not replace a running parent's location.

Each alternative owns one travel record. Its integer position uses legacy
logical map units, not displayed star-map units. During exploration it anchors
the current star. Flight stores the system-local position and the existing
one-based entry/save markers. These markers remain common during migration
because navigation, system entry, encounters and old saves use them. In
particular, flight.loadOrbit is a restore instruction, not the live orbit.
There is no parallel live copy in SIS or the global campaign payload.

SolarSystemLocation additionally identifies the star and contains optional live
orbit and surface state. Entering orbit preserves flight; landing preserves
flight and orbit. The lander borrows the actual surface position by reference,
so its updates do not need synchronization. Visit destruction clears the surface
before returning to orbit; the orbit scope clears orbit before returning to
flight. Landing without an orbit throws bad_optional_access. Body identity uses
WorldId, including its optional moon slot.

Conversation and combat children leave the travel variant intact. Their return
resumes the same parent and the same borrowed location. Combat arena positions,
headings and velocities remain in the combat simulation. Mode suspend/resume
continues to handle rendering independently of this data.

C code temporarily borrows PLAYER_TRAVEL_POSITION through
GetPlayerTravelPosition. The borrow expires at a top-level realm change or load.
Native exploration uses a typed reference through its explicit session. The old
GAME_STATE and SIS_STATE coordinate fields remain only as serialization buffers:
ExportPlayerTravelPosition fills them when saving, and both modern and legacy
loaders call ImportPlayerTravelPosition after reading. Import discards old live
visit state; the dispatcher selects the restored realm and reconstructs it.
Existing file layout, integer units and one-based markers are unchanged. Save
summaries continue to contain the map position without becoming authoritative.
Arcade compatibility bindings supply isolated travel storage, never campaign
location storage.

The remaining legacy flight descriptors (display stamp, velocity and viewport
transforms) still belong to the existing engine path. This change moves travel
position and live orbit/surface location; it does not move rendering resources
back out of World or introduce the physics-policy design tracked in issue #51.

Issue #52 tracks replacing the legacy restore markers with an explicit saved
exploration location and encoding old formats only at the save boundary.
