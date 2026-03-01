# Flight Computer Firmware

Embedded C firmware for the on-board flight controller.

## Target Hardware

TBD — likely STM32F4 series (ARM Cortex-M4)

## Building

```bash
cd flight-computer
make        # Build firmware
make flash  # Flash to board (requires connected hardware)
make clean  # Clean build artifacts
```

## Code Standards

This is safety-critical code. All contributions **must** follow:

- No dynamic memory allocation (`malloc`/`free` banned)
- No recursion
- All loops must have bounded iteration counts
- All variables initialised before use
- Compile with `-Wall -Werror` — zero warnings allowed
- Every function must have a clear, documented purpose

## Directory Structure

```
flight-computer/
├── src/        # C source files
├── include/    # Header files
├── tests/      # Unit tests (run on host, not on target)
├── Makefile    # Build configuration
└── README.md   # You are here
```
