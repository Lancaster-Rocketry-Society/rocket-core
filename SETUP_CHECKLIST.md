# 🚀 GitHub Organisation Setup Checklist

Your org exists — here's exactly what to do now, in order.

## Phase 1: Repos (Do Today)

- [ ] **Create `rocket-core` repo** in the org
  - Initialise with README (you'll replace it with the one provided)
  - Set visibility to **Public**
  - Add the MIT licence
- [ ] **Create `rocket-docs` repo** in the org
  - Initialise with README
  - Set visibility to **Public**

- [ ] **Push the provided files** into each repo:
  ```bash
  # For rocket-core
  git clone https://github.com/<your-org>/rocket-core.git
  cd rocket-core
  # Copy all the provided files into this directory
  # (README.md, .gitignore, LICENSE, .github/, flight-computer/, 
  #  ground-station/, simulations/, shared/, tools/, workshops/, docs/)
  git add .
  git commit -m "Initial repository structure and documentation"
  git push origin main
  ```
  Repeat for `rocket-docs`.

## Phase 2: Branch Protection (Do Today)

- [ ] Go to `rocket-core` → Settings → Branches → Add rule
  - Branch name pattern: `main`
  - ✅ Require a pull request before merging
  - ✅ Require approvals: **1**
  - ✅ Dismiss stale pull request approvals when new commits are pushed
  - ✅ Require status checks to pass before merging (enable later when CI exists)
  - ✅ Include administrators (so even you go through review)
  - Save

## Phase 3: Teams (Do Today)

- [ ] Go to Org → Settings → Teams → New Team
- [ ] Create teams:
  - `leads` — you + subsystem leads (Maintainer role on repos)
  - `flight-software` — anyone working on the flight computer
  - `ground-station` — anyone working on ground systems
  - `simulations` — data analysis and simulation work
- [ ] Add members to appropriate teams as they join

## Phase 4: Project Board (Do Today)

- [ ] Go to Org page → Projects tab → New Project
- [ ] Select **Board** view
- [ ] Name it: "Rocket 1 — Development"
- [ ] Set up columns:
  - 📋 Backlog
  - 📌 To Do (this sprint)
  - 🔨 In Progress
  - 👀 In Review
  - ✅ Done
- [ ] Link the project to `rocket-core` repo

## Phase 5: Labels (Do Today)

- [ ] Go to `rocket-core` → Issues → Labels
- [ ] Delete the default labels (or keep useful ones like `bug`)
- [ ] Create these labels:

  | Label | Colour | Description |
  |-------|--------|-------------|
  | `flight-software` | #1d76db | Flight computer firmware |
  | `ground-station` | #0e8a16 | Ground station software |
  | `simulation` | #d93f0b | Simulations & analysis |
  | `documentation` | #c5def5 | Docs and guides |
  | `good-first-issue` | #7057ff | Good for newcomers |
  | `enhancement` | #a2eeef | New feature |
  | `bug` | #d73a4a | Something broken |
  | `discussion` | #fbca04 | Needs team input |
  | `blocked` | #b60205 | Waiting on dependency |
  | `launch-pad` | #f9d0c4 | Launch pad controller |
  | `shared-protocol` | #e4e669 | Cross-subsystem protocol work |

## Phase 6: Seed Your First Issues (Do This Week)

Create these starter issues to give people something to work on immediately:

### good-first-issue (onboarding tasks)
- [ ] "Set up a basic Makefile for flight-computer directory"
- [ ] "Create a Python script that plots altitude vs time from a CSV file"
- [ ] "Research STM32F4 boards and write a comparison in docs"
- [ ] "Add a ground-station requirements.txt with initial dependencies"

### Architecture tasks
- [ ] "Design the telemetry packet format (shared/protocols)"
- [ ] "Define the flight state machine transitions and document in ADR"
- [ ] "Research LoRa radio modules for telemetry link"

### Simulation tasks
- [ ] "Implement basic 1D trajectory simulation (drag + gravity + thrust)"
- [ ] "Create a script to parse OpenRocket CSV exports"

## Phase 7: Invite Members (This Week)

- [ ] Go to Org → People → Invite member
- [ ] Invite everyone on the software team by GitHub username or email
- [ ] Assign them to the appropriate team(s)
- [ ] Point them to `rocket-docs/onboarding/getting-started.md`

## Phase 8: Later Improvements (When Needed)

- [ ] Add GitHub Actions CI for compilation checks (when there's code to compile)
- [ ] Add linting (clang-format for C, black/ruff for Python)
- [ ] Set up GitHub Discussions if async design conversations grow
- [ ] Consider splitting repos if subsystems diverge significantly
- [ ] Add GitHub Pages for a public project site (optional)
