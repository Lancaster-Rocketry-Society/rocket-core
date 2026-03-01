# Workshop 01: Introduction to Embedded C for Rocketry

## Session Goals

By the end of this session, you will:
- Understand why we use C for flight software
- Set up your development environment
- Write a simple program that reads a sensor and prints values to serial
- Understand basic GPIO and I2C concepts

## Prerequisites

- A laptop with Linux, macOS, or Windows (WSL recommended)
- Git installed and configured
- The repo cloned locally

## Part 1: Why C?

Our flight computer runs on a microcontroller — a tiny processor with limited memory (typically 256KB flash, 64KB RAM). We need a language that:

- Compiles to native machine code (no interpreter, no VM)
- Gives us precise control over memory and timing
- Has no garbage collector that could pause at the wrong moment
- Has decades of proven use in safety-critical systems

C gives us all of this. It's also what NASA, ESA, and every major aerospace company uses for embedded systems.

## Part 2: Environment Setup

```bash
# Ubuntu / WSL
sudo apt update
sudo apt install gcc-arm-none-eabi gdb-multiarch minicom

# macOS (with Homebrew)
brew install --cask gcc-arm-embedded
brew install minicom
```

## Part 3: Your First Embedded Program

[TODO: Add hands-on exercise once target hardware is selected]

## Part 4: Reading a Sensor

[TODO: Add I2C sensor reading exercise]

## Homework

- Read the Architecture Overview in `/docs/architecture/ARCHITECTURE.md`
- Look at the flight state machine diagram and think about what data each state needs
- Pick an issue labelled `good-first-issue` and have a go

## Resources

- [The Embedded Rust Book](https://docs.rust-embedded.org/book/) — good concepts even if we're using C
- [STM32 Reference Manual](https://www.st.com/resource/en/reference_manual/) — once we select hardware
- NASA's "Power of Ten" rules: https://en.wikipedia.org/wiki/The_Power_of_10:_Rules_for_Developing_Safety-Critical_Code
