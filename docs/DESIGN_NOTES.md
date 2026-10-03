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

### Simulation separate from presentation

The simulation core (`liftsim_core`) is a static library with no graphics dependency, and the view layer only reads from it. The physics and control logic can therefore be tested headless, and they are deterministic: same seed, same step, same result. A simulated hour runs in a fraction of a second, which is what makes the KPI study below practical. The renderer could be replaced without touching the logic.

### Fixed timestep

The simulation always advances in 1/120 s steps and an accumulator carries leftover frame time forward. Physics results therefore do not depend on the frame rate, and 32× fast-forward is 32× more steps rather than bigger, less accurate ones.

### One source of truth for geometry

Every world-space position (shaft centers, sheave location, counterweight travel) comes from `BuildingLayout`. The scene, people, picking and HUD labels cannot disagree about where something is.

### Procedural models

Every mesh is built in code from three unit primitives (cube, sphere, cylinder) transformed by model matrices, so there are no asset files to license or load.

## Verification

Requirements are listed with their sources in [REQUIREMENTS.md](REQUIREMENTS.md). `tests/SimTests.cpp` verifies each one by ID, and CI runs the tests on every push.

## Findings from the simulation

Thirty minutes of simulated traffic per case, 4 cars, 24 upper floors (`./build/liftsim_kpi`, seed 5). "Unserved" counts passengers still waiting or riding when the run ends.

| Pattern | Arrivals / 5 min | Avg wait | Max wait | Avg journey | Unserved at end |
|---|---|---|---|---|---|
| Up-peak | 30 | 9.2 s | 54 s | 52.2 s | 6 |
| Up-peak | 60 | 25.6 s | 107 s | 98.5 s | 28 |
| Up-peak | 90 | 73.8 s | 265 s | 169.2 s | 65 |
| Lunch | 30 | 17.3 s | 69 s | 56.8 s | 3 |
| Lunch | 60 | 41.5 s | 128 s | 100.3 s | 20 |
| Lunch | 90 | 99.9 s | 304 s | 179.7 s | 66 |
| Interfloor | 30 | 22.9 s | 73 s | 57.5 s | 7 |
| Interfloor | 60 | 89.4 s | 306 s | 153.2 s | 28 |
| Down-peak | 60 | 57.5 s | 162 s | 108.6 s | 30 |

In up-peak the group saturates between about 60 and 90 persons per 5 minutes, 6–9 % of the 960-person population. At 60 the average wait is still acceptable; at 90 it triples and queues build.

A hand calculation of the up-peak round-trip time (Barney & Al-Sharif; CIBSE Guide D) agrees:

| Quantity | Formula | Value |
|---|---|---|
| Passengers per trip, P | 80 % of 21 | 16 |
| Upper floors, N | | 24 |
| Probable stops, S | N·(1 − (1 − 1/N)^P) | 11.9 |
| Highest reversal floor, H | N − Σ (i/N)^P, i = 1…N−1 | 23.0 (83 m) |
| Time to pass one floor at rated speed, t_v | 3.6 m / 2.5 m/s | 1.44 s |
| Time lost per stop, t_s | one-floor flight 4.52 s − t_v + doors 1.8 + 2.4 s | 7.28 s |
| Transfer time per passenger, t_p | | 1.2 s |
| Round-trip time | 2H·t_v + (S + 1)·t_s + 2P·t_p | 198 s |
| ... adding the simulator's 1.5 s door hold per stop | + (S + 1)·1.5 s | 218 s |
| Handling capacity | 300 s · P · 4 cars / RTT | 88–97 persons / 5 min (9–10 %) |
| Interval | RTT / 4 cars | 50–55 s |

The formula assumes every car leaves the lobby 80 % full, so it is an upper bound; in the simulation cars leave as soon as boarding stops, which explains saturation starting somewhat lower.

Office guidance has traditionally targeted an up-peak handling capacity of 12–15 % per 5 minutes and an interval of about 30 s; more recent surveys show mixed lunchtime traffic is often the busiest period, which is why the lunch pattern is studied too. Either way, four cars fall well short: a 25-storey office tower needs about six cars, or a split into low-rise and high-rise zones.

## Simplifications (and what a production model would add)

| Here | Real installation |
|---|---|
| 1:1 roping, rigid ropes | 2:1 roping is common; rope stretch and bounce affect leveling |
| Drive modelled as an ideal speed-tracking loop | motor, inverter and encoder dynamics; load weighing for pre-torque |
| ETA dispatching with a heuristic route estimate | destination dispatch, energy-aware and learning dispatchers |
| Single-deck cars, one lobby | double-deck, sky lobbies, zoning |
| Safety gear, rope brake and machine brake as constant retardation | gear type, rail friction and brake torque determine the retardation curve |
| Mechanical safety devices modelled as logic | certified electrical safety circuits (SIL-rated) |
