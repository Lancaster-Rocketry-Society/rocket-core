#!/usr/bin/env python3
# =============================================================================
# tools/analyze_flight.py — post-flight analysis of a FLIGHTnn.CSV log
#
# Usage:
#   python3 analyze_flight.py FLIGHT00.CSV
#   python3 analyze_flight.py FLIGHT00.CSV --plot flight.png --export ork.csv
#
# Prints a flight summary (apogee, max speed, max g, boost time, descent
# rate, flight time), saves a 3-panel plot, and can export a time-aligned
# altitude/velocity CSV for comparison against an OpenRocket simulation
# (File > Export simulated data in OpenRocket, then line the two up from
# t=0 at launch).
#
# Requires: pandas, matplotlib   (pip install pandas matplotlib)
# =============================================================================
import argparse
import re
import sys

import pandas as pd


def parse_log(path):
    """Returns (DataFrame, events dict, comment lines)."""
    events = {}
    comments = []
    rows = []
    header = None
    with open(path, "r", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                comments.append(line)
                m = re.match(r"#\s*EVENT\s+t=(\d+)\s+(\w+)(.*)", line)
                if m:
                    events[m.group(2)] = {
                        "t_ms": int(m.group(1)),
                        "detail": m.group(3).strip(),
                    }
                continue
            if line.startswith("t_ms,"):
                header = line.split(",")
                continue
            if header and line[0].isdigit():
                rows.append(line.split(","))
    if header is None or not rows:
        raise SystemExit(f"{path}: no CSV data found — is this a flight log?")
    df = pd.DataFrame(rows, columns=header)
    for c in df.columns:
        if c != "state":
            df[c] = pd.to_numeric(df[c], errors="coerce")
    return df, events, comments


def summarize(df, events):
    out = []
    t0 = events.get("LAUNCH", {}).get("t_ms")
    if t0 is None:
        # fall back to first BOOST row
        boost = df[df.state == "BST"]
        t0 = int(boost.t_ms.iloc[0]) if len(boost) else int(df.t_ms.iloc[0])
        out.append("note: no LAUNCH event found, using first BOOST row")
    df = df.copy()
    df["t_s"] = (df.t_ms - t0) / 1000.0

    apo_i = df.alt_agl_m.idxmax()
    apo_alt = df.alt_agl_m.loc[apo_i]
    apo_t = df.t_s.loc[apo_i]
    out.append(f"apogee            : {apo_alt:7.1f} m AGL at t+{apo_t:.2f} s")

    vmax = df.vspeed_ms.max()
    out.append(f"max vertical speed: {vmax:7.1f} m/s "
               f"({vmax * 3.6:.0f} km/h, Mach {vmax / 343.0:.2f})")

    gmax = df.accmag_ms2.max()
    out.append(f"max |accel|       : {gmax:7.1f} m/s^2 ({gmax / 9.81:.1f} g)")

    boost = df[df.state == "BST"]
    if len(boost) > 1:
        out.append(f"boost duration    : {boost.t_s.iloc[-1] - boost.t_s.iloc[0]:7.2f} s")

    dsc = df[(df.state == "DSC") & (df.alt_agl_m > 5)]
    if len(dsc) > 10:
        rate = -dsc.vspeed_ms.median()
        out.append(f"descent rate      : {rate:7.1f} m/s (median under canopy)")

    if "LANDED" in events:
        out.append(f"flight time       : "
                   f"{(events['LANDED']['t_ms'] - t0) / 1000.0:7.1f} s "
                   f"(launch -> landing detect)")
    for name in ("LAUNCH", "BURNOUT", "APOGEE", "LANDED"):
        if name in events:
            out.append(f"event {name:8s}    : t+"
                       f"{(events[name]['t_ms'] - t0) / 1000.0:.2f} s "
                       f"{events[name]['detail']}")
    return df, out


def make_plot(df, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, axes = plt.subplots(3, 1, figsize=(11, 9), sharex=True)
    fig.suptitle("LURS Rocket 1 — flight log", fontsize=13)

    inflight = df[(df.t_s > -10) & (df.state != "LND")]
    tmax = inflight.t_s.max() + 15 if len(inflight) else df.t_s.max()

    ax = axes[0]
    ax.plot(df.t_s, df.alt_agl_m, color="#1f77b4", lw=1.2)
    ax.set_ylabel("altitude AGL (m)")
    ax.grid(alpha=0.3)

    ax = axes[1]
    ax.plot(df.t_s, df.vspeed_ms, color="#ff7f0e", lw=0.9)
    ax.set_ylabel("vertical speed (m/s)")
    ax.grid(alpha=0.3)

    ax = axes[2]
    ax.plot(df.t_s, df.accmag_ms2, color="#2ca02c", lw=0.9)
    ax.set_ylabel("|accel| (m/s²)")
    ax.set_xlabel("time since launch (s)")
    ax.grid(alpha=0.3)

    for ax in axes:
        ax.set_xlim(-10, tmax)
    plt.tight_layout()
    plt.savefig(path, dpi=130)
    print(f"plot saved        : {path}")


def export_openrocket(df, path):
    """Time-aligned altitude/velocity from launch, for overlaying against an
    OpenRocket export (Time, Altitude, Vertical velocity columns)."""
    out = df[df.t_s >= 0][["t_s", "alt_agl_m", "vspeed_ms", "accmag_ms2"]]
    out = out.rename(columns={
        "t_s": "time_s",
        "alt_agl_m": "altitude_m",
        "vspeed_ms": "vertical_velocity_ms",
        "accmag_ms2": "accel_magnitude_ms2",
    })
    out.to_csv(path, index=False)
    print(f"OpenRocket-comparison CSV: {path}")


def main():
    ap = argparse.ArgumentParser(description="Analyze a LURS FLIGHTnn.CSV log")
    ap.add_argument("log", help="path to FLIGHTnn.CSV from the SD card")
    ap.add_argument("--plot", default=None, help="save plot PNG here")
    ap.add_argument("--export", default=None,
                    help="save time-aligned CSV for OpenRocket comparison")
    args = ap.parse_args()

    df, events, comments = parse_log(args.log)
    for c in comments[:4]:
        print(c)
    print(f"data rows         : {len(df)}")

    df, summary = summarize(df, events)
    for s in summary:
        print(s)

    if args.plot:
        make_plot(df, args.plot)
    if args.export:
        export_openrocket(df, args.export)
    return 0


if __name__ == "__main__":
    sys.exit(main())
