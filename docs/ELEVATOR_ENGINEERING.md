# Elevator engineering model

This document explains the models in `src/sim/`, the equations they use and where the parameter values come from.

## 1. The installation

| Parameter | Value | Note |
|---|---|---|
| Floors | 25 (lobby + 24 office floors) | 3.6 m floor-to-floor, 86.4 m travel |
| Cars | 4, rated 1600 kg / 21 persons | 75 kg design mass per person |
| Rated speed | 2.5 m/s | typical for 20–30 storey offices |
| Acceleration / jerk | 1.0 m/s² / 1.5 m/s³ | typical comfort limits for offices |
| Machine | gearless PM synchronous, 640 mm sheave, 1:1 roping | deflector sheave spaces the ropes |
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
T_car = (m_car + m_load) · (g + a)          rope tension, car side
T_cwt =  m_cwt           · (g − a)          rope tension, counterweight side
F     = T_car − T_cwt + m_rot · a           force the machine supplies at the sheave rim
τ     = F · r                                machine torque
P     = F · v                                mechanical power (+ motoring, − regenerating)
```

`m_rot` is the rotating inertia of the motor and sheaves, expressed as an equivalent mass at the rope. Electrical power divides motoring power by the drive efficiency and multiplies regenerated power by it. As a result:

- An **empty car going up** or a **full car going down** *regenerates*, because the load is being lowered.
- A **full car going up** or an **empty car going down** draws power.

**Rope slip:** the ropes grip the sheave by friction. The Euler–Eytelwein (capstan) equation says they hold while

```
T_high / T_low  ≤  e^(μ·α)
```

where μ is the effective groove friction (0.2 for an undercut groove) and α is the wrap angle (π rad). The HUD shows the live ratio against this limit, and a unit test checks that a fully loaded car accelerating at the limit never slips.

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

## 8. Sources

- **EN 81-20:2020.** *Safety rules for the construction and installation of lifts, Part 20: Passenger and goods passenger lifts.* Source for the governor tripping speed (≥ 115 % rated), progressive safety gear retardation (0.2 g–1.0 g), stopping accuracy (±10 mm), unintended car movement protection, and the 75 kg design mass per person.
- **ASME A17.1 / CSA B44.** *Safety Code for Elevators and Escalators.* The North American counterpart.
- **CIBSE Guide D: Transportation Systems in Buildings** (Chartered Institution of Building Services Engineers, 2020). Flight-time formula, passenger transfer times, traffic patterns, handling-capacity targets.
- **G. Barney and L. Al-Sharif, *Elevator Traffic Handbook: Theory and Practice*, 2nd ed., Routledge, 2016.** Traffic patterns, round-trip time, group control.
- **R. D. Peters, "Ideal Lift Kinematics", *Elevator Technology 6*, IAEE, 1995.** Equations of motion for jerk-limited lift travel.
- **Euler–Eytelwein (capstan) equation.** Standard mechanics result for belt and rope friction.

Numbers are typical values taken from these references for an educational model. They are not certified design values.
