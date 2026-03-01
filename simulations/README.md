# Simulations & Analysis

Python-based flight simulation, data analysis, and hardware-in-the-loop testing.

## Setup

```bash
cd simulations
pip install -r requirements.txt
```

## Contents

- **Trajectory simulation** — predict flight path given motor data, mass, drag
- **Post-flight analysis** — compare predicted vs actual flight data
- **Sensor fusion prototyping** — Kalman filter development and tuning
- **Hardware-in-the-loop** — feed simulated sensor data to flight computer for testing

## Dependencies

See `requirements.txt` (numpy, matplotlib, scipy at minimum)
