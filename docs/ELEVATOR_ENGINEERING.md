# Elevator engineering model

This document explains the models in `src/sim/`, the equations they use and where the parameter values come from.

## 1. The installation

| Parameter | Value | Note |
|---|---|---|
| Floors | 25 (lobby + 24 office floors) | 3.6 m floor-to-floor, 86.4 m travel |
| Cars | 4, rated 1600 kg / 21 persons | 75 kg design mass per person |
| Rated speed | 2.5 m/s | typical for 20–30 storey offices |
| Acceleration / jerk | 1.0 m/s² / 1.5 m/s³ | typical comfort limits for offices |
| Machine | gearless PM synchronous, 640 mm sheave, 1:1 roping, single wrap | 520 mm deflector sheave spaces the ropes |
| Hoist ropes | 6 × 13 mm steel, 0.58 kg/m each | 3.5 kg/m in total |
| Compensation | chains, 3.48 kg/m | equal to the hoist ropes' 6 × 0.58 kg/m |
| Buffers | oil (energy-dissipating) | 0.42 m stroke |
| Counterweight | car mass + 45 % of rated load | balances the car at ~45 % load (40–50 % is typical) |
| Car | 2.1 × 1.6 m = 3.36 m² | EN 81-20 Table 6 allows at most 3.56 m² for 1600 kg |
| Overhead / pit | 5.0 m / 2.0 m | derived in section 5 |

Values marked *assumption* in `ElevatorSpec.h` (door timings, car mass, rotating mass, drive efficiency, run-by, creep speed) are reasonable engineering choices without a single citable source.

## 2. Motion control (`MotionController`)

The drive follows a speed pattern that keeps acceleration and jerk (the rate of change of acceleration) within comfort limits. The controller has three stages and a final-stop ramp:

1. Profile generator. The fastest speed from which the car can still stop within the remaining distance *d*, following an S-curve:

   Stopping distance from speed *v*:  `s(v) = v² / (2A) + v·A / (2J)`. This is exact when the deceleration reaches *A*, i.e. for `v ≥ A²/J` (about 0.6 m/s here). Below that it overestimates, which is the safe side.

   Solving the quadratic for *v* gives the speed pattern:  `v(d) = ½ ( −A²/J + √(A⁴/J² + 8·A·d) )`

   The pattern is planned at 90 % of the limits so the regulator keeps some authority in reserve.

2. Speed regulator. `a_request = feed-forward + (v_ref − v_predicted) / τ`, with τ = 0.25 s. The feed-forward term is the rate at which the pattern itself is falling, so the car tracks it without lag.

   `v_predicted` is a look-ahead: acceleration cannot drop to zero instantly, so a car that is still accelerating keeps gaining speed (`a²/2J`) and distance before it can start braking. Planning from that predicted state instead of the current one removes overshoot on short trips.

3. Jerk limiter. Acceleration may change by at most `J·Δt` per step.

4. Final stop. The pattern ends in a short creep at 0.03 m/s. From creep speed *v* the drive ramps speed to exactly zero with a symmetric triangle of deceleration (jerk −J, then +J), which lasts `T = 2√(v/J)` and covers `v·T/2`. The ramp starts that distance before the floor, so it ends on the floor; only then does the brake drop, onto a stationary sheave. Because deceleration starts and ends at zero, the stop stays within the jerk limit, and the test counts the brake-drop step too. Every trip stops within 0.3 mm; EN 81-20 requires ±10 mm. (Many modern drives land directly without a creep; the creep here is a design choice.)

Flight time, used by the dispatcher: for trips long enough to reach rated speed, `t = d/v + v/a + a/j` (CIBSE Guide D). Shorter trips peak at a lower speed.

## 3. Traction physics (`Car::UpdateTraction`)

With 1:1 roping, the car and counterweight hang on opposite sides of the traction sheave and accelerate in opposite directions. With car acceleration *a* (positive up):

```
T_car = M_car_side · (g + a)                rope tension, car side
T_cwt = M_cwt_side · (g − a)                rope tension, counterweight side
F     = T_car − T_cwt + m_rot · a           force the machine supplies at the sheave rim
τ     = F · r                                machine torque
P     = F · v                                mechanical power (+ motoring, − regenerating)
```

`M_car_side` and `M_cwt_side` are everything hanging from each side of the sheave at the current car position (`Car::Suspension`):

```
M_car_side = m_car + m_load + ρ_rope·(h + H − y) + ρ_chain·(p + y) + cable
M_cwt_side = m_cwt          + ρ_rope·(h + y)     + ρ_chain·(p + H − y)
```

Here H is the travel (86.4 m), y the car position, h the rope length above the car at the top landing, and p the chain loop depth. Without compensation the hoist ropes alone swing the balance by 2·ρ·H ≈ 600 kg over the travel. That is why rope weight must be compensated above roughly 30–40 m of travel. With chains of the same linear mass, the terms in y cancel, and a unit test checks that the swing drops from 554 kg to 48 kg (the remaining 48 kg is the traveling cable).

`m_rot` is the rotating inertia of the motor and sheaves, expressed as an equivalent mass at the rope. Electrical power divides motoring power by the drive efficiency and multiplies regenerated power by it. As a result:

- An empty car going up or a full car going down regenerates, because the load is being lowered.
- A full car going up or an empty car going down draws power.

### Rope slip

The ropes grip the sheave by friction. The Euler–Eytelwein (capstan) equation says they hold while

```
T_high / T_low  ≤  e^(f·α)
```

where *f* is the effective groove friction and α is the wrap angle. EN 81-50 uses this inequality for traction calculations, with a rope-on-steel coefficient μ = 0.1 for normal operation; the shape of an undercut groove multiplies it by a factor of about 2, giving f ≈ 0.2.

Only EN 81-50's normal-operation case is checked (full car, full acceleration). The standard also requires an *emergency-braking* case (with a lower μ) and a *stalled* case, where the ropes must slip if the counterweight is held on its buffer. Neither is modelled, so the simulator does not claim EN 81-50 compliance.

The wrap angle is computed from the roping geometry (`ElevatorSpec::RopeTangentAngle`) rather than assumed to be 180°:

- The car rope drops from the front of the traction sheave. The counterweight rope drops from the far side of a deflector sheave 2.0 m below and 1.45 m behind it.
- Between the two sheaves the rope runs on their external tangent. For tangent points `P = C + r·n` on both circles, `(C_sheave − C_deflector)·n = −(r_sheave − r_deflector)`.
- Solving that gives a wrap of 158°, typical for a single-wrap machine with a deflector, and a traction limit of e^(0.2·2.76) = 1.74.
- The renderer draws the ropes along the same tangent, so the picture and the physics agree.

The HUD and inspector show the live ratio against this limit. A unit test checks that a fully loaded car accelerating at the limit never slips (normal operation).

## 4. Pit buffers

Energy-dissipating (oil) buffers must have a stroke of at least `0.0674·v²`. That is the distance to stop from 115 % of rated speed at an average of 1 g: `(1.15v)² / (2g) = 0.0674·v²`. At 2.5 m/s this is 0.42 m, and the buffers in the pit are drawn with that plunger stroke.

## 5. Hoistway overhead and pit

The hoistway geometry is derived rather than drawn by eye (`ElevatorSpec`, `BuildingLayout`):

- Overhead (top landing to machine-room floor). If the counterweight lands on its fully compressed buffer, the car rises run-by + stroke = 0.2 + 0.42 m above the top landing. EN 81 top-clearance rules then require `1.0 + 0.035·v²` = 1.22 m of free height above the car roof (2.6 m). Total 4.44 m, plus a 0.25 m slab, so 5.0 m is used. The office roof is one storey (3.6 m) above the top landing, so the machine room stands on a raised plinth, as on real buildings.
- Counterweight top position. With the car at the bottom landing the counterweight is at its highest. It stays below the deflector even if the car then over-travels onto its compressed buffer and the counterweight jumps: run-by + stroke + 0.035·v² + a 0.1 m margin = 0.94 m.
- Pit. On its fully compressed buffer the car sill is 0.62 m below the bottom landing, and the toe guard (EN 81-20: at least 0.75 m) reaches 1.37 m. A 2.0 m pit leaves about 0.6 m clear below it.
- Rope length. The car rope from the crosshead hitch to the sheave is 2.95 m at the top landing. The machine is placed from that number, so the rope weight in the physics and the drawn ropes agree.

## 6. Doors (`DoorOperator`)

The operator is a state machine: `Closed → Opening → Open → Closing → Closed`. Panel travel uses a smoothstep curve, so the panels accelerate and brake like a real operator. Closing is slower than opening because door kinetic energy is limited by code. If the light curtain is interrupted while closing, the doors reverse immediately. The car door drives the landing door at the same floor through a mechanical coupler, which is why landing doors in the scene only open when a car is there.

## 7. Safety chain (`SafetyChain`, `Car::UpdateSafety`)

The safety chain is modelled as a series circuit: the drive can only run when every contact is closed.

| Contact | Opens when | Consequence |
|---|---|---|
| Door locks | any car or landing door is not fully closed and locked | the car cannot start |
| Overspeed governor, car moving down | speed exceeds 115 % of rated speed | the governor rope sets the car's safety gear on the rails (0.5 g average; EN 81-20 allows 0.2–1.0 g) |
| Overspeed governor, car moving up | speed exceeds 115 % of rated speed | ascending car overspeed protection, here a rope brake (0.5 g, an assumption; EN 81-20 caps an empty car at 1 g) |
| UCM monitor | the car moves more than 0.15 m from the landing with its doors open | the machine brake stops it; EN 81-20 requires the stop within 1.2 m of the landing |

Every trip latches the car out of service. Each device is modelled as a constant average retardation.

A car's progressive safety gear only grips when the car moves down. EN 81-20 therefore requires separate protection against ascending overspeed: a rope brake, a second machine brake, counterweight safety gear, or a bidirectional car safety gear. The ascending case is the common one: an empty car is lighter than its counterweight, so if the brake fails it rises.

Fault injection (`O`): with motor torque and brake lost, the system freewheels toward the heavier side, accelerating at

```
a = (M_cwt_side − M_car_side) · g / (M_car_side + M_cwt_side + m_rot)
```

where the side masses include ropes, chains and cable (section 3). An empty car rises: the governor trips at 2.875 m/s and the rope brake stops it. A full car falls and the safety gear stops it. Near balance the acceleration is tiny, so the simulation enforces at least 0.4 m/s² to keep the demo short; this is a shortcut, not physics. A reset (`R`) relevels the car to the nearest landing so trapped passengers can get out.

## 8. Group control (`ElevatorSystem`)

Each car runs selective collective control. A moving car answers car calls and same-direction hall calls ahead of it, then turns around at the farthest opposite-direction call (the "elevator algorithm", or SCAN). A car only takes a new stop if it can still brake comfortably for it (`Car::CanStopAt`). The committed floor (the nearest floor the car could still stop at) is what dispatching uses as the car's position.

Across the group, every new hall call goes to the car with the lowest estimated time of arrival:

```
ETA = remaining door time
    + distance / v_rated + (stops + 1) · (v/a + a/j)     travel, with a start/stop overhead per run
    + stops · stop_time                                   doors + transfers at intermediate stops
    + penalty if the car is > 80 % loaded (it will bypass hall calls)
```

If the car is heading away from the call, the distance includes the trip to its farthest committed stop and back. Every second, active calls are re-evaluated and moved to a better car if the improvement is more than 5 s, unless the assigned car is already braking for that call.

Other behaviours:

- Full-load bypass: above 80 % load a car skips hall calls.
- Left behind: passengers who could not board press the button again.
- Hall button reopen: a hall button pressed in the direction of a car that is closing its doors reopens them.
- Lobby parking: during up-peak, idle cars return to the lobby.

## 9. Traffic (`TrafficGenerator`)

Passengers arrive as a Poisson process: the time between arrivals is exponentially distributed with mean 300 s / rate, where the rate is given in persons per 5 minutes (the usual unit in traffic analysis).

The up-peak and lunch mixes follow surveys of modern office buildings by Peters Research, which CIBSE Guide D also uses. Down-peak mirrors up-peak, and the interfloor mix is an assumption.

Waiting time is measured from arrival at the landing until boarding, so it includes being left behind by a full car. The CIBSE definition ends when the responding car begins to open its doors, so the simulator's figures run slightly longer.

| Pattern | Incoming (from lobby) | Outgoing (to lobby) | Interfloor |
|---|---|---|---|
| Up-peak | 85 % | 10 % | 5 % |
| Down-peak | 5 % | 85 % | 10 % |
| Lunch | 45 % | 45 % | 10 % |
| Interfloor | 10 % | 10 % | 80 % |

## 10. Accuracy

| Area | Status |
|---|---|
| Motion (S-curve, limits, leveling) | Matches the ideal-kinematics equations; verified for every trip. |
| Traction forces, torque, power, regeneration | First-principles, with rope, chain and cable masses included. |
| Rope slip check | Euler–Eytelwein with the geometric wrap angle, normal-operation case only (constant f, no groove-geometry derivation, no emergency-braking or stalled case). |
| Governor, safety gear, rope brake, UCM, buffers | Thresholds from EN 81-20; each stopping device is modelled as a constant average retardation. |
| Hoistway geometry | Overhead, pit and counterweight clearances derived from the buffer stroke, run-by and EN 81 clearance rules (section 5). |
| Doors | Timings and behaviour typical; the eased panel profile stands in for a real operator's motion profile. |
| Dispatch | ETA assignment with collective control. Real products use proprietary algorithms (destination dispatch, learning). |
| Not modelled | EN 81-50 emergency-braking and stalled traction cases, rope elasticity and car bounce, 2:1 roping, motor/inverter electrical dynamics, pre-torque load weighing, ride vibration, door kinetic-energy limits, building sway. |

Parameter values are typical figures from the references below. This is an educational model, not a certified design tool.

## 11. Sources

These are also listed in the app: press `I`, or open a component with `Tab`. Clicking a source opens it.

- [EN 81-20:2020](https://www.evs.ee/en/evs-en-81-20-2020), *Safety rules for lifts, Part 20.* Governor tripping speed (≥ 115 % rated), safety gear retardation (0.2 g–1.0 g), ascending car overspeed protection, UCM protection (stop within 1.2 m), stopping accuracy (±10 mm), buffer stroke (0.0674 v²), toe guard (≥ 0.75 m), maximum car area (Table 6), 75 kg per person.
- [KONE, EN 81-20 fact sheet](https://www.kone.com.au/Images/pdf_Safety%20Standard%20EN81-20%20Fact%20Sheet_tcm46-29613.pdf). UCM protection through the hoisting machine's brake; ascending car overspeed protection.
- [ASME A17.1-2022 / CSA B44-22](https://webstore.ansi.org/standards/csa/csaasmea172022b44), *Safety Code for Elevators and Escalators.* The North American counterpart.
- [CIBSE Guide D:2020 (6th ed.)](https://cibse.org/knowledge-research/knowledge-portal/guide-d-transportation-systems-in-buildings-2020), *Transportation Systems in Buildings.* Flight time, round-trip time, traffic patterns, handling capacity, waiting-time definition.
- [Lift Passenger Demand in Office Buildings, Elevator World](https://elevatorworld.com/?p=41334). Office traffic mixes: up-peak 85/10/5, lunch 45/45/10.
- [R. D. Peters, *Ideal Lift Kinematics* (1995)](https://liftescalatorlibrary.org/paper_indexing/abstract_pages/00000329.html). Jerk-limited equations of motion.
- [G. Barney & L. Al-Sharif, *Elevator Traffic Handbook*, 2nd ed. (2016)](https://www.routledge.com/Elevator-Traffic-Handbook-Theory-and-Practice/Barney-Al-Sharif/p/book/9781032179650). Traffic analysis, round-trip time, group control.
- [L. Wiek, *On Friction for Traction of Elevators* (1996)](https://liftescalatorlibrary.org/paper_indexing/abstract_pages/00000362.html) and [Theory of Rope Traction, Elevator World](https://elevatorworld.com/?p=26460). Euler–Eytelwein traction.
- [US 8,360,212 B2](https://patents.google.com/patent/US8360212). "a travel height of just 30–40 meters necessitates compensation of the imbalance."
- [Oleo International: elevator buffers](https://www.oleoelevator.com/elevator-safety/). Buffer strokes and energy absorption.
