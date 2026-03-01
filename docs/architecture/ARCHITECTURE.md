# System Architecture Overview

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        LAUNCH SITE                               │
│                                                                   │
│  ┌─────────────────┐    Radio Link    ┌──────────────────────┐   │
│  │  FLIGHT COMPUTER │ ◄─────────────► │    GROUND STATION    │   │
│  │  (On-board)      │   (LoRa/UHF)   │    (Laptop + Radio)  │   │
│  │                  │                  │                      │   │
│  │  - Sensors       │                  │  - Live telemetry    │   │
│  │  - State machine │                  │  - Data logging      │   │
│  │  - Parachute     │                  │  - Launch control    │   │
│  │    deployment    │                  │  - Abort capability  │   │
│  │  - Data logging  │                  │                      │   │
│  └─────────────────┘                  └──────────────────────┘   │
│                                                │                  │
│  ┌─────────────────┐                          │                  │
│  │  LAUNCH PAD      │ ◄──────────────────────┘                  │
│  │  CONTROLLER      │      Wired / Radio                        │
│  │                  │                                            │
│  │  - Ignition      │                                            │
│  │  - Arming        │                                            │
│  │  - Continuity    │                                            │
│  └─────────────────┘                                            │
└─────────────────────────────────────────────────────────────────┘

                    POST-FLIGHT
        ┌──────────────────────────┐
        │  ANALYSIS & SIMULATION   │
        │                          │
        │  - Flight data review    │
        │  - Trajectory comparison │
        │  - Model validation      │
        └──────────────────────────┘
```

## Subsystem Breakdown

### Flight Computer
- **Language:** C (bare-metal embedded)
- **Target hardware:** TBD (likely STM32F4 or similar ARM Cortex-M)
- **Responsibilities:**
  - Read sensors (accelerometer, gyroscope, barometer, GPS)
  - Run flight state machine (pad idle → armed → boost → coast → apogee → drogue → main → landed)
  - Deploy parachute charges at correct flight phases
  - Log all sensor data to onboard storage
  - Transmit telemetry to ground station via radio

### Ground Station
- **Language:** Python and/or C++
- **Runs on:** Laptop at launch site
- **Responsibilities:**
  - Receive and decode telemetry from flight computer
  - Display real-time altitude, velocity, acceleration, GPS position
  - Log all received data
  - Provide launch control interface (arm, ignite, abort)
  - Post-flight data export

### Launch Pad Controller
- **Language:** C (microcontroller)
- **Responsibilities:**
  - Ignition relay control
  - Arming/disarming with physical safety switch
  - Continuity checking of igniter circuit
  - Communication with ground station

### Simulations & Analysis
- **Language:** Python
- **Responsibilities:**
  - Pre-flight trajectory prediction
  - Post-flight data analysis and visualisation
  - Sensor fusion algorithm development and testing
  - Hardware-in-the-loop test harness

## Communication Protocol

All subsystems communicate using a common packet format defined in `/shared/protocols/`.

**Packet structure (TBD — to be designed as first task):**
```
[SYNC][LENGTH][TYPE][PAYLOAD][CRC16]
```

## Flight State Machine

```
  PAD_IDLE ──► ARMED ──► BOOST ──► COAST ──► APOGEE
                                                │
                                          DROGUE_DEPLOY
                                                │
                                          MAIN_DEPLOY
                                                │
                                             LANDED

  Any state ──► ABORT (triggered by ground station or anomaly detection)
```

## Design Decisions

Major design decisions are recorded as ADRs in `/docs/architecture/decisions/`.

Format: `NNNN-title.md` (e.g., `0001-use-stm32f4-for-flight-computer.md`)
