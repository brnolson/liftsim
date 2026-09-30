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
| Compensation | chains, 3.48 kg/m | chains are usual up to ~2.5 m/s |
| Buffers | oil (energy-dissipating) | 0.42 m stroke |
| Counterweight | car mass + 45 % of rated load | balances the car at ~45 % load |

## 2. Motion control (`MotionController`)

A real elevator drive does not "teleport to the next floor". It follows a speed pattern that keeps acceleration and jerk (the rate of change of acceleration) within comfort limits. The controller has three stages, the same structure found in commercial drives:

1. **Profile generator.** The fastest speed from which the car can still stop within the remaining distance *d*, following an S-curve:

   Stopping distance from speed *v*:  `s(v) = v² / (2A) + v·A / (2J)`

   Solving the quadratic for *v* gives the speed pattern:  `v(d) = ½ ( −A²/J + √(A⁴/J² + 8·A·d) )`

   The pattern is planned at 90 % of the limits so the regulator keeps some authority in reserve.

2. **Speed regulator.** `a_request = feed-forward + (v_ref − v_predicted) / τ`, with τ = 0.25 s. The feed-forward term is the rate at which the pattern itself is falling, so the car tracks it without lag.

3. **Jerk limiter.** Acceleration may change by at most `J·Δt` per step.

**Look-ahead:** acceleration cannot drop to zero instantly, so a car that is still accelerating will keep gaining speed (`a²/2J`) and distance before it can start braking. Planning from that predicted state instead of the current one removes overshoot on short trips.

**Leveling:** the final approach is at a 0.03 m/s creep. The brake drops inside a 3 mm window, and the residual error is recorded and shown in the HUD. EN 81-20 requires ±10 mm stopping accuracy.

**Flight time** (used by the dispatcher): for trips long enough to reach rated speed, `t = d/v + v/a + a/j` (CIBSE Guide D). Shorter trips peak at a lower speed.

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

Here H is the travel (86.4 m), y the car position, h the rope length above the car at the top landing, and p the chain loop depth. Without compensation the hoist ropes alone swing the balance by 2·ρ·H ≈ **600 kg** over the travel. That is why rope weight must be compensated above roughly 30–40 m of travel. With chains of the same linear mass, the terms in y cancel, and a unit test checks that the swing drops from 554 kg to 48 kg (the remaining 48 kg is the traveling cable).

`m_rot` is the rotating inertia of the motor and sheaves, expressed as an equivalent mass at the rope. Electrical power divides motoring power by the drive efficiency and multiplies regenerated power by it. As a result:

- An **empty car going up** or a **full car going down** *regenerates*, because the load is being lowered.
- A **full car going up** or an **empty car going down** draws power.

**Rope slip:** the ropes grip the sheave by friction. The Euler–Eytelwein (capstan) equation says they hold while

```
T_high / T_low  ≤  e^(μ·α)
```

where f is the effective groove friction (0.2 for an undercut groove) and α is the wrap angle. EN 81-50 uses this same inequality for traction calculations.

**The wrap angle is computed from the roping geometry** (`ElevatorSpec::RopeTangentAngle`), not assumed to be 180°:

- The car rope drops from the front of the traction sheave. The counterweight rope drops from the far side of a deflector sheave 2.0 m below and 1.45 m behind it.
- Between the two sheaves the rope runs on their external tangent. For tangent points `P = C + r·n` on both circles, `(C_sheave − C_deflector)·n = −(r_sheave − r_deflector)`.
- Solving that gives a wrap of **158°**, typical for a single-wrap machine with a deflector, and a traction limit of e^(0.2·2.76) = 1.74.
- The renderer draws the ropes along the same tangent, so the picture and the physics agree.

The HUD and inspector show the live ratio against this limit. A unit test checks that a fully loaded car accelerating at the limit never slips.

## 3b. Pit buffers

Energy-dissipating (oil) buffers must have a stroke of at least `0.0674·v²`. That is the distance to stop from 115 % of rated speed at an average of 1 g: `(1.15v)² / (2g) = 0.0674·v²`. At 2.5 m/s this is **0.42 m**, and the buffers in the pit are drawn with that plunger stroke.

## 4. Doors (`DoorOperator`)

The operator is a state machine: `Closed → Opening → Open → Closing → Closed`. Panel travel uses a smoothstep curve, so the panels accelerate and brake like a real operator. Closing is slower than opening because door kinetic energy is limited by code. If the **light curtain** is interrupted while closing, the doors reverse immediately. The car door drives the landing door at the same floor through a mechanical coupler, which is why landing doors in the scene only open when a car is there.

## 5. Safety chain (`SafetyChain`, `Car::UpdateSafety`)

The safety chain is modelled as a series circuit: the drive can only run when every contact is closed.

| Contact | Opens when | Consequence |
|---|---|---|
| Door locks | any car or landing door is not fully closed and locked | the car cannot start |
| Overspeed governor | car speed exceeds 115 % of rated speed | the safety gear grips the rails (constant ~0.5 g retardation), car out of service |
| UCM monitor | the car moves more than 0.15 m from the landing with its doors open | emergency stop, car out of service |

**Fault injection** (`O`): with motor torque and brake lost, the system freewheels toward the heavier side, accelerating at

```
a = (m_cwt − m_car − m_load) · g / (m_car + m_load + m_cwt + m_rot)
```

so an empty car *rises*. The governor trips at 2.875 m/s and the safety gear stops the car. A reset (`R`) relevels the car to the nearest landing so trapped passengers can get out.

## 6. Group control (`ElevatorSystem`)

**Per car, selective collective control.** A moving car answers car calls and same-direction hall calls ahead of it, then turns around at the farthest opposite-direction call (the "elevator algorithm", or SCAN). A car only takes a new stop if it can still brake comfortably for it (`Car::CanStopAt`). The **committed floor** (the nearest floor the car could still stop at) is what dispatching uses as the car's position.

**Across the group, ETA dispatching.** Every new hall call goes to the car with the lowest estimated time of arrival:

```
ETA = remaining door time
    + distance / v_rated + (stops + 1) · (v/a + a/j)     travel, with a start/stop overhead per run
    + stops · stop_time                                   doors + transfers at intermediate stops
    + penalty if the car is > 80 % loaded (it will bypass hall calls)
```

If the car is heading away from the call, the distance includes the trip to its farthest committed stop and back. Every second, active calls are re-evaluated and moved to a better car if the improvement is more than 5 s, unless the assigned car is already braking for that call.

**Other behaviours:**
- **Full-load bypass:** above 80 % load a car skips hall calls.
- **Left behind:** passengers who could not board press the button again.
- **Hall button reopen:** a hall button pressed in the direction of a car that is closing its doors reopens them.
- **Lobby parking:** during up-peak, idle cars return to the lobby.

## 7. Traffic (`TrafficGenerator`)

Passengers arrive as a **Poisson process**: the time between arrivals is exponentially distributed with mean 300 s / rate, where the rate is given in persons per 5 minutes (the usual unit in traffic analysis).

| Pattern | Incoming (from lobby) | Outgoing (to lobby) | Interfloor |
|---|---|---|---|
| Up-peak | 85 % | 10 % | 5 % |
| Down-peak | 5 % | 85 % | 10 % |
| Lunch | 40 % | 40 % | 20 % |
| Interfloor | 10 % | 10 % | 80 % |

## 8. How accurate is it?

| Area | Status |
|---|---|
| Motion (S-curve, limits, leveling) | Matches the ideal-kinematics equations; verified for every trip. |
| Traction forces, torque, power, regeneration | First-principles, with rope, chain and cable masses included. |
| Rope slip check | Euler–Eytelwein with the geometric wrap angle (the EN 81-50 method in simplified form: constant f, no groove-geometry derivation). |
| Governor, safety gear, UCM, buffers | Thresholds from EN 81-20; the safety gear is modelled as a constant average retardation. |
| Doors | Timings and behaviour typical; the eased panel profile stands in for a real operator's motion profile. |
| Dispatch | ETA assignment with collective control. Real products use proprietary algorithms (destination dispatch, learning). |
| **Not modelled** | Rope elasticity and car bounce, 2:1 roping, motor/inverter electrical dynamics, pre-torque load weighing, ride vibration, door kinetic-energy limits, building sway. |

Parameter values are typical figures from the references below. This is an educational model, not a certified design tool.

## 9. Sources

These are also listed in the app: press **I**, or open a component with **Tab**. Clicking a source opens it.

- **[EN 81-20:2020](https://www.evs.ee/en/evs-en-81-20-2020)**, *Safety rules for lifts, Part 20.* Governor tripping speed (≥ 115 % rated), safety gear retardation (0.2 g–1.0 g), stopping accuracy (±10 mm), UCM protection, buffer stroke (0.0674 v²), 75 kg per person.
- **[ASME A17.1-2022 / CSA B44-22](https://webstore.ansi.org/standards/csa/csaasmea172022b44)**, *Safety Code for Elevators and Escalators.* The North American counterpart.
- **[CIBSE Guide D, 5th ed. (2020)](https://www.cibsejournal.com/news/fifth-edition-of-guide-d-launched/)**, *Transportation Systems in Buildings.* Flight time, transfer times, traffic patterns, handling capacity.
- **[R. D. Peters, *Ideal Lift Kinematics* (1995)](https://liftescalatorlibrary.org/paper_indexing/abstract_pages/00000329.html).** Jerk-limited equations of motion.
- **[G. Barney & L. Al-Sharif, *Elevator Traffic Handbook*, 2nd ed. (2016)](https://www.routledge.com/Elevator-Traffic-Handbook-Theory-and-Practice/Barney-Al-Sharif/p/book/9781032179650).** Traffic analysis, round-trip time, group control.
- **[L. Wiek, *On Friction for Traction of Elevators* (1996)](https://liftescalatorlibrary.org/paper_indexing/abstract_pages/00000362.html)** and **[Theory of Rope Traction, Elevator World](https://elevatorworld.com/?p=26460).** Euler–Eytelwein traction.
- **[US 8,360,212 B2](https://patents.google.com/patent/US8360212).** "a travel height of just 30–40 meters necessitates compensation of the imbalance."
- **[Oleo International: elevator buffers](https://www.oleoelevator.com/elevator-safety/).** Buffer strokes and energy absorption.
