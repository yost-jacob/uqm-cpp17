# Runtime architecture and migration map

This maps the live implementation on `feature/solar-system-locations`, starting
at the campaign mode dispatcher. It is a subsystem map, not a complete call graph.
Super Melee also enters combat outside this campaign dispatcher.

Colors describe the implementation boundary, not gameplay quality:

- **Green — native:** explicit C++ ownership and a native interface.
- **Yellow — mixed:** native coordination or execution that still relies on legacy state, handles, or routines.
- **Orange — adapter:** C/C++ translation, borrowed buffers, opaque handles, or flag conversion.
- **Red — legacy:** procedural loops, globals, packed records, or callback tables remain authoritative.
- **Gray — foundation:** shared resource/platform infrastructure; not necessarily a candidate for replacement.

A green object can still depend on red storage. Dashed arrows indicate shared
state or supporting services; solid arrows indicate the main execution path.

## Execution from the mode dispatcher

```mermaid
flowchart TD
    D["GameSession::run<br/>owns currentMode; consumes ModeResult"]:::native
    M["Concrete GameMode::run implementations<br/>native lifetime; legacy activity flags"]:::mixed
    D --> M
    M --> H["HyperSpaceMode"]:::mixed
    M --> S["SolarSystemMode"]:::mixed
    M --> E["EncounterMode"]:::mixed
    M --> O["LoadMenuMode"]:::mixed

    H --> B["Battle loop<br/>battle.c / process.c<br/>also used for HyperSpace movement"]:::legacy
    H --> HP["hyper.c<br/>navigation and encounters"]:::legacy
    S --> EX["ExploreSolarSys<br/>interplanetary / orbit / scan / lander"]:::legacy
    E --> EC["RaceCommunication / VisitStarBase<br/>communication, encounters, story actions"]:::legacy
    EC --> B

    B --> SE["ship.cpp / cyborg.cpp<br/>native dispatch; procedural execution / prediction"]:::mixed
    SE --> SH["Ship base + 28 concrete classes<br/>owned descriptor / private values"]:::native
    SH --> EL["C element engine<br/>projectile callbacks / collision / retirement"]:::legacy
    B --> EL

    EX --> GEN["SystemBehavior dispatch<br/>native Chmmr; 27 legacy special systems"]:::mixed
    EX --> WA["world_c.cpp / discovery_state_c.cpp<br/>descriptor, handle and progress translation"]:::adapter
    GEN --> WA
    GEN --> SB["SolarSystem-owned behavior<br/>borrowed CampaignState reference"]:::native
    WA --> WO["SolarSystem / World / EnergyBehavior<br/>runtime ownership and selected energy rules"]:::native

    O --> MN["Native MenuResult + C menu bridge<br/>game_menu.cpp / game_session_c.h"]:::adapter
    B -. nested menus .-> MN
    EX -. nested menus .-> MN
    EC -. nested menus .-> MN
    MN --> UI["Legacy menu input / drawing / save selection"]:::legacy
    MN -. exit request after stack unwinds .-> D
    D --> EXIT["SessionExit to starcon.c<br/>title / load / shutdown / failure"]:::adapter
    EXIT -->|load request only| LOAD["Next runCampaign loads at safe boundary<br/>legacy LoadGame decoder"]:::legacy

    SH -. handles and shared services .-> RES["Existing assets / UIO / graphics / sound / SDL2"]:::foundation
    EL -.-> RES
    EX -.-> RES

    classDef native fill:#d9f2df,stroke:#27783d,color:#102a17;
    classDef mixed fill:#fff1bf,stroke:#9a7400,color:#342700;
    classDef adapter fill:#ffe0bd,stroke:#af5c00,color:#3c2100;
    classDef legacy fill:#f8d8da,stroke:#a43f48,color:#3b1419;
    classDef foundation fill:#e8edf2,stroke:#667582,color:#1e2933;
```

There is currently **no separate campaign CombatMode**. Encounter and other
legacy paths can enter combat below the mode boundary. Modes run synchronously;
this is not an asynchronous task graph. A menu requests a session exit, existing
frames return, and the dispatcher releases its mode before campaign teardown or
loading. The decoder itself remains nontransactional.

## State ownership and compatibility access

```mermaid
flowchart LR
    SESSION["GameSession"]:::native -->|shared_ptr| STATE["GameState"]:::native
    STATE -->|owns| PLAYER["PlayerEntity<br/>identity / campaign interface"]:::native
    STATE -->|shared_ptr| WS["WorldState<br/>physical world progress"]:::native
    PLAYER --> CAM["CampaignState<br/>typed story access / personal discoveries"]:::mixed
    PLAYER --> PAYLOAD["PlayerStorage<br/>authoritative legacy payload"]:::legacy
    CAM -. story bytes still live here .-> PAYLOAD
    C["Legacy gameplay callers"]:::legacy --> BRIDGE["GetLegacyGlobData / GLOBAL macros<br/>borrowed payload access"]:::adapter
    BRIDGE --> PAYLOAD
    W["SolarSystem / World"]:::native -->|shared_ptr| WS
    W -. energy interaction receives campaign .-> CAM
    SAVE["Legacy save / load encoding"]:::legacy -.-> PAYLOAD
    SAVE -. archive compatibility conversion .-> WS

    classDef native fill:#d9f2df,stroke:#27783d,color:#102a17;
    classDef mixed fill:#fff1bf,stroke:#9a7400,color:#342700;
    classDef adapter fill:#ffe0bd,stroke:#af5c00,color:#3c2100;
    classDef legacy fill:#f8d8da,stroke:#a43f48,color:#3b1419;
```

Ownership is encapsulated more thoroughly than access. C callers can still
mutate the authoritative payload through the compatibility view. Campaign story
flags are views over the existing packed bytes, not a second copy. Shared
ownership preserves lifetime; it does not make the game concurrent or multiplayer.

## Data and ship construction

```mermaid
flowchart LR
    SJ["base/starmap.json"]:::foundation --> MAP["StarMap<br/>vector of native StarDesc values"]:::native
    MAP --> SB["star_map_c.cpp<br/>STAR_DESC snapshot / sentinels"]:::adapter
    SB --> NAV["Legacy navigation / generator selection"]:::legacy
    TJ["base/ships.json"]:::foundation --> TUNE["ShipTuningCatalog<br/>validated immutable tuning"]:::native
    TUNE --> FACT["createShip species switch<br/>unique_ptr of concrete Ship"]:::native
    FACT --> LOADER["load_ship / free_ship<br/>unique ownership at C boundary<br/>partial-load rollback"]:::adapter
    LOADER --> ASSETS["Existing image / sound / text resources"]:::foundation
    LOADER --> BORROW["C gameplay borrows RACE_DESC<br/>ship_owner links to native owner"]:::adapter
    BORROW --> EXEC["ship.cpp / cyborg.cpp<br/>direct native hooks; no dispatch wrappers"]:::mixed

    classDef native fill:#d9f2df,stroke:#27783d,color:#102a17;
    classDef mixed fill:#fff1bf,stroke:#9a7400,color:#342700;
    classDef adapter fill:#ffe0bd,stroke:#af5c00,color:#3c2100;
    classDef legacy fill:#f8d8da,stroke:#a43f48,color:#3b1419;
    classDef foundation fill:#e8edf2,stroke:#667582,color:#1e2933;
```

The old ship code-resource machinery and three C ship-dispatch wrappers are
removed. The remaining ship asset adapter has real C callers and selective asset
transfer contracts; it is not redundant simply because its implementation is C++.

## Reading the remaining work

| Area | Current boundary | What deeper encapsulation would involve |
| --- | --- | --- |
| Mode selection | Native dispatcher; mixed mode bodies | Move activity/transition decisions away from global flags and into explicit results. |
| Solar-system generation | Scoped native system/behavior; Chmmr overrides; 27 legacy special systems | Migrate remaining handlers through the borrowed campaign reference, then retire their tables and C bridge as callers migrate. |
| Navigation | Legacy HyperSpace and interplanetary loops | Own navigation runtime state behind a native interface while preserving transitions. |
| Encounters and communication | Legacy nested loops and story-state access | Separate encounter decisions from UI/execution and make exits explicit. |
| Combat | Native ships and callers over a legacy element engine | Encapsulate battle/element lifetime, collisions and projectile behaviors; preserve retirement and reincarnation ordering. |
| Menus and saves | Typed exit request around legacy UI and decoder | Migrate menu callers; separately address transactional state decoding. |
| State | Native lifetime around a legacy payload | Replace borrowed global access incrementally without introducing duplicate authoritative state. |

The Battle / ProjectileBehavior design in [combat/README.md](combat/README.md)
is an executable test prototype, **not** the production combat implementation
shown above. Likewise, native EnergyBehavior covers selected energy-site rules;
it is separate from system-level orbital/story dispatch. See the
[system behavior migration](solar-system/system-behavior.md) for the live default
and Chmmr migration, scoped owner and remaining legacy adapters.

## Source anchors

- [Dispatcher](../../sc2/src/uqm/game_mode.cpp), [mode bodies and campaign lifecycle](../../sc2/src/uqm/gameplay_modes.cpp), [outer campaign caller](../../sc2/src/uqm/starcon.c).
- [State ownership](../../sc2/src/uqm/game_state.hpp), [payload adapter](../../sc2/src/uqm/game_state.cpp), [world runtime](../../sc2/src/uqm/world.hpp).
- [World adapter](../../sc2/src/uqm/world_c.cpp), [generator selection](../../sc2/src/uqm/gendef.c), [menu adapter](../../sc2/src/uqm/game_menu.cpp).
- [Ship execution](../../sc2/src/uqm/ship.cpp), [cyborg prediction](../../sc2/src/uqm/cyborg.cpp), [native ship](../../sc2/src/uqm/ship_behavior.hpp), [asset boundary](../../sc2/src/uqm/loadship.cpp).
- [Star catalogue adapter](../../sc2/src/uqm/star_map_c.cpp), [ship tuning loader](../../sc2/src/uqm/ship_tuning_c.cpp).
