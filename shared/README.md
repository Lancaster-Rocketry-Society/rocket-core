# Shared Definitions

Common protocol definitions, packet formats, and constants used across all subsystems.

**Changes here affect the flight computer, ground station, and launch pad controller.** PRs modifying this directory require review from the Principal Software Engineer.

## Contents

- `protocols/` — Communication packet definitions and serialisation
- `constants.h` / `constants.py` — Shared constants (pin mappings, frequencies, limits)

## Protocol Design Principles

- Fixed-size packets where possible (predictable memory usage)
- CRC16 on all packets (detect corruption)
- Explicit versioning (so ground station can talk to different firmware versions)
