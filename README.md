# 🚀 Lancaster Rocket Society — Core Repository

The central codebase for Lancaster Rocket Society's flight software, ground station, simulations, and tooling.

## Repository Structure

```
rocket-core/
├── flight-computer/     # Embedded C — flight controller firmware
│   ├── src/             # Source files
│   ├── include/         # Header files
│   └── tests/           # Unit tests
├── ground-station/      # Ground telemetry & control software
│   ├── src/
│   └── tests/
├── simulations/         # Flight simulation & data analysis (Python)
├── shared/              # Shared definitions across subsystems
│   └── protocols/       # Packet formats, communication protocols
├── tools/               # Build scripts, flash utilities, dev helpers
├── workshops/           # Teaching materials for society sessions
├── docs/                # Architecture docs & design decisions
│   └── architecture/    # System diagrams & design docs
└── .github/             # Issue templates, CI workflows, CODEOWNERS
```

## Getting Started

### Prerequisites

- **GCC ARM toolchain** — for compiling flight computer firmware
- **Python 3.10+** — for simulations, tooling, and ground station components
- **Git** — obviously

### Clone the repo

```bash
git clone https://github.com/<your-org>/rocket-core.git
cd rocket-core
```

### First time setup

```bash
# Install Python dependencies for simulations & tools
pip install -r simulations/requirements.txt

# Check the flight computer toolchain is available
arm-none-eabi-gcc --version
```

## How We Work

### Branching Strategy

- `main` is always stable and reviewed — **never push directly to main**
- Create feature branches from `main`: `feature/telemetry-parser`, `fix/barometer-calibration`
- Open a Pull Request when your work is ready for review
- At least **one approving review** is required before merging
- Use **squash merge** to keep the history clean

### Naming Conventions

| Type | Format | Example |
|------|--------|---------|
| Feature branch | `feature/<description>` | `feature/lora-telemetry` |
| Bug fix branch | `fix/<description>` | `fix/altitude-overflow` |
| Experiment | `experiment/<description>` | `experiment/kalman-filter` |
| Issue title | Clear, actionable | "Implement barometer driver for BMP280" |

### Labels

| Label | Use for |
|-------|---------|
| `flight-software` | Flight computer firmware |
| `ground-station` | Ground systems |
| `simulation` | Simulations & analysis |
| `documentation` | Docs, READMEs, architecture |
| `good-first-issue` | Suitable for new members |
| `bug` | Something broken |
| `enhancement` | New feature or improvement |
| `discussion` | Needs team input before work starts |
| `blocked` | Waiting on something else |

### Code Review

All code goes through PR review. When reviewing:

1. **Does it compile/run?** Check the CI status.
2. **Is it readable?** Could a new member understand this in 6 months?
3. **Is it safe?** Especially for flight software — no unbounded loops, no dynamic allocation, no undefined behaviour.
4. **Is it tested?** Does it have tests or a clear plan for testing?

## Safety Standards

Flight software follows a subset of safety-critical coding practices:

- **No dynamic memory allocation** in flight code (`malloc`/`free` are banned)
- **No recursion** — all functions must have bounded call depth
- **All loops must have a bounded iteration count**
- **All variables initialised before use**
- **No compiler warnings** — treat warnings as errors (`-Wall -Werror`)

These rules exist because our code fires explosive charges. Take them seriously.

## Contributing

1. Check the [GitHub Projects board](https://github.com/orgs/<your-org>/projects) for available tasks
2. Assign yourself to an issue (or ask a lead to assign you)
3. Create a branch, do the work, open a PR
4. Respond to review feedback
5. Once approved, a maintainer will merge

New to the society? Look for issues labelled `good-first-issue`.

## Workshop Materials

Session materials live in `/workshops/`. Each session has its own numbered markdown file. Work through them at your own pace or attend live sessions.

## Docs & Design Decisions

Architecture docs and design decisions live in `/docs/`. If you're making a significant design choice, write it up as an ADR (Architecture Decision Record) so future members understand *why* we built things this way.

## Contact

- **Principal Software Engineer:** Morgan
- **GitHub Org:** [Lancaster Rocket Society](https://github.com/<your-org>)

## Licence

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.
