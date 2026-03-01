# Ground Station

Software for receiving telemetry, displaying live flight data, and controlling launch operations.

## Tech Stack

- **Python 3.10+** for data processing and UI
- **C++** (optional, for performance-critical decoding if needed)

## Running

```bash
cd ground-station
pip install -r requirements.txt
python src/main.py
```

## Features (Planned)

- [ ] Serial/radio telemetry receiver
- [ ] Real-time altitude, velocity, acceleration display
- [ ] GPS position plotting
- [ ] Flight data logging to CSV
- [ ] Launch control panel (arm, ignite, abort)
- [ ] Post-flight data export and replay
