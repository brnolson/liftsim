# Requirements and verification

Each requirement has an ID, the source it is derived from, and the test that verifies it (`tests/SimTests.cpp`; test names carry the IDs). CI runs every test on each push.

| ID | Requirement | Source | Verified by |
|---|---|---|---|
| REQ-MOT-01 | Car speed, acceleration and jerk shall not exceed 2.5 m/s, 1.0 m/s² and 1.5 m/s³ on any trip, including the final stop when the brake drops. | CIBSE Guide D; Peters (1995) | Every trip, 1–24 floors, both directions |
| REQ-MOT-02 | The car shall stop within ±10 mm of the landing. | EN 81-20 (stopping accuracy) | Every trip, 1–24 floors, both directions |
| REQ-MOT-03 | The car shall not overshoot the target landing. | Ride quality | Every trip, 1–24 floors, both directions |
| REQ-DSP-01 | The dispatcher's flight-time estimate shall be within 25 % of the simulated run time. | CIBSE Guide D flight-time formula | 1, 3, 8 and 24-floor trips |
| REQ-DOR-01 | Closing doors shall reverse when the light curtain is interrupted. | EN 81-20; ASME A17.1 | Interrupt the beam mid-close |
| REQ-SAF-01 | The car shall not start while any door is unlocked (safety chain open). | EN 81-20; ASME A17.1 | Command a run with doors open |
| REQ-SAF-02a | On ascending overspeed the governor shall trip at 115 % of rated speed and the ascending car overspeed protection (rope brake) shall stop the car. | EN 81-20 | Drive fault, empty car (rises) |
| REQ-SAF-02b | On descending overspeed the governor shall trip at 115 % of rated speed and the car safety gear shall stop the car. | EN 81-20 | Drive fault, full car at the top (falls) |
| REQ-SAF-03 | Car movement away from the landing with doors unlocked shall be detected and the machine brake shall stop the car within 1.2 m of the landing. | EN 81-20 (UCM); KONE fact sheet | Drive fault with doors open |
| REQ-TRC-01 | In normal operation the rope tension ratio shall stay below e^(f·α) at full load and full acceleration. | Euler–Eytelwein; EN 81-50 normal-operation case only | Full car, full travel |
| REQ-TRC-02 | Compensation shall limit the hoist-rope imbalance swing over full travel. | Industry practice above 30–40 m travel (US 8,360,212) | Compensated vs uncompensated |
| REQ-ENG-01 | Lowering a net load (an empty car going up) shall regenerate energy. | Traction physics | Empty car sent up |
| REQ-DSP-02 | Every passenger shall be delivered to their destination. | Functional | 120-passenger up-peak |
| REQ-DSP-03 | No car shall exceed its rated load, and no passenger shall be stranded. | EN 81-20 (rated load); functional | One simulated hour of lunch traffic |

EN 81-50's emergency-braking and stalled traction cases are not modelled, so there is no requirement for them.

This mirrors how regulated software is developed (for example IEC 62304 for medical device software):

- requirements are written down with a rationale;
- every requirement has a verification;
- the link between the two is traceable in both directions.
