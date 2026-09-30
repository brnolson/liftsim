# Design decisions and findings

## Architecture

```
            ┌────────────────────────────┐
 input ───► │  sim::ElevatorSystem       │  fixed 120 Hz step, deterministic (seeded RNG)
            │   Car × 4                  │  no OpenGL, no window, no globals
            │    MotionController        │
            │    DoorOperator            │  ◄── tests/SimTests.cpp drives this headless
            │    SafetyChain / Traction  │
            │   TrafficGenerator         │
            └─────────────┬──────────────┘
                          │ read-only
            ┌─────────────▼──────────────┐
            │  view::SceneBuilder         │  sim state ─► DrawItems + RayBoxes
            │  view::PeopleView           │  logical passenger ─► walking figure
            └─────────────┬──────────────┘
            ┌─────────────▼──────────────┐
            │  render::Renderer           │  shadow ─► scene ─► post
            │  ui::Hud                    │
            └────────────────────────────┘
```

**Simulation separated from presentation.** The simulation core (`liftsim_core`) is a separate static library with no graphics dependency, and the view layer only reads from it. This means:

- The physics and control logic are **unit-tested headless** and **deterministic**: same seed, same step, same result.
- A simulated hour runs in a fraction of a second, which makes KPI studies practical (below).
- The renderer can be replaced or removed without touching the logic.

**Fixed timestep.** The simulation always advances in 1/120 s steps and an accumulator carries leftover frame time forward. Physics results therefore do not depend on the frame rate, and 32× fast-forward is 32× more steps rather than bigger, less accurate ones.

**One source of truth for geometry.** Every world-space position (shaft centers, sheave location, counterweight travel) comes from `BuildingLayout`. The scene, people, picking and HUD labels cannot disagree about where something is.

**Procedural models.** Every mesh is built in code from three unit primitives (cube, sphere, cylinder) transformed by model matrices. There are no asset files to license or load, and every vertex on screen can be traced to a line of code.

## Verification

Requirements are listed with their sources in [REQUIREMENTS.md](REQUIREMENTS.md). `tests/SimTests.cpp` verifies each one by ID, and CI runs the tests on every push. The tests check:

- the motion limits and leveling accuracy for every possible trip in both directions;
- safety interlocks (no motion with doors open);
- governor and UCM trips on injected faults;
- no rope slip at full load, using the geometric wrap angle;
- compensation chains cancelling the rope imbalance;
- energy regeneration;
- passenger conservation over an hour of mixed traffic (nobody stranded, no overload).

## Findings from the simulation

Thirty minutes of simulated traffic per case, 4 cars, 24 upper floors (`./build/liftsim_kpi`, seed 5):

| Pattern | Arrivals / 5 min | Avg wait | Max wait | Avg journey | Still queued |
|---|---|---|---|---|---|
| Up-peak | 30 | 11.2 s | 50 s | 56.5 s | 5 |
| Up-peak | 60 | 27.9 s | 133 s | 103.9 s | 28 |
| Up-peak | 90 | 70.1 s | 220 s | 165.5 s | 70 |
| Lunch | 30 | 19.1 s | 73 s | 54.7 s | 4 |
| Lunch | 60 | 39.2 s | 170 s | 95.8 s | 28 |
| Interfloor | 30 | 23.3 s | 107 s | 57.7 s | 8 |
| Interfloor | 60 | 99.8 s | 285 s | 157.2 s | 34 |
| Down-peak | 60 | 53.6 s | 154 s | 107.3 s | 31 |

**The group saturates at about 60 persons per 5 minutes, about 6 % of the 960-person population.** Above that, queues grow without bound and the cars run at close to 100 % utilization.

CIBSE Guide D recommends an up-peak handling capacity of 12 % or more per 5 minutes for offices. A hand calculation of the round-trip time agrees with the simulation:

- 16 passengers per trip;
- about 12 probable stops;
- about 86 m highest reversal;
- which gives an RTT of about 230 s and a handling capacity of about 83 persons per 5 minutes at full load.

The practical conclusion: **a 25-storey office tower needs either about 6 cars or a low-rise/high-rise zoned split.** This is the kind of result a traffic analysis exists to produce, and why the simulator measures KPIs rather than just animating.

## Simplifications (and what a production model would add)

| Here | Real installation |
|---|---|
| 1:1 roping, rigid ropes | 2:1 roping is common; rope stretch and bounce affect leveling |
| Drive modelled as an ideal speed-tracking loop | motor, inverter and encoder dynamics; load weighing for pre-torque |
| ETA dispatching with a heuristic route estimate | destination dispatch, energy-aware and learning dispatchers |
| Single-deck cars, one lobby | double-deck, sky lobbies, zoning |
| Safety gear as constant retardation | gear type and rail friction determine the retardation curve |
| Mechanical safety devices modelled as logic | certified electrical safety circuits (SIL-rated) |
