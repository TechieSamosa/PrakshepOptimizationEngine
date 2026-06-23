import json
import matplotlib.pyplot as plt
import os
import numpy as np

def generate_graphs():
    data_file = "data/telemetry_dump.json"
    if not os.path.exists(data_file):
        print(f"Data file not found: {data_file}")
        return
        
    with open(data_file, "r") as f:
        telemetry = json.load(f)
        
    if not telemetry:
        print("No telemetry data to plot.")
        return

    # Extract time series
    times = [d["time"] for d in telemetry]
    altitudes = [d["altitude"] / 1000.0 for d in telemetry] # in km
    velocities = [d["velocity_magnitude"] for d in telemetry] # in m/s
    q_dyns = [d["dynamic_pressure"] / 1000.0 for d in telemetry] # in kPa
    
    # Create directory
    os.makedirs("docs/figures", exist_ok=True)
    
    # Configure plotting style for academic paper
    plt.style.use('seaborn-v0_8-paper')
    plt.rcParams.update({
        "font.family": "serif",
        "font.serif": ["Times", "Palatino", "serif"],
        "axes.titlesize": 14,
        "axes.labelsize": 12,
        "xtick.labelsize": 10,
        "ytick.labelsize": 10,
        "legend.fontsize": 10,
        "figure.dpi": 300,
        "grid.alpha": 0.5,
        "grid.linestyle": "--"
    })
    
    # ----------------------------------------------------
    # Figure 1: Altitude vs. Time (Max-Q highlighted)
    # ----------------------------------------------------
    plt.figure(figsize=(8, 5))
    plt.plot(times, altitudes, color="#004c99", linewidth=2.5, label="Altitude (km)")
    
    # Find Max-Q index
    max_q_idx = np.argmax(q_dyns)
    max_q_time = times[max_q_idx]
    max_q_alt = altitudes[max_q_idx]
    
    plt.scatter(max_q_time, max_q_alt, color="red", s=80, zorder=5, label=f"Max-Q ({max_q_time:.1f}s)")
    plt.annotate(
        "Max-Q", 
        (max_q_time, max_q_alt),
        textcoords="offset points", 
        xytext=(-20, 15), 
        ha='center', 
        fontsize=11, 
        fontweight='bold', 
        color='darkred'
    )
    
    plt.title("Vehicle Altitude vs. Time")
    plt.xlabel("Time (s)")
    plt.ylabel("Altitude (km)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig("docs/figures/fig1_altitude.png")
    plt.close()

    # ----------------------------------------------------
    # Figure 2: Inertial Velocity vs. Altitude
    # ----------------------------------------------------
    plt.figure(figsize=(8, 5))
    plt.plot(altitudes, velocities, color="#990000", linewidth=2.5)
    plt.title("Inertial Velocity vs. Altitude")
    plt.xlabel("Altitude (km)")
    plt.ylabel("Velocity (m/s)")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("docs/figures/fig2_velocity.png")
    plt.close()

    # ----------------------------------------------------
    # Figure 3: Dynamic Pressure over Time
    # ----------------------------------------------------
    plt.figure(figsize=(8, 5))
    plt.plot(times, q_dyns, color="#006600", linewidth=2.5)
    plt.axvline(x=max_q_time, color='r', linestyle='--', alpha=0.7, label=f"Max-Q at {max_q_time:.1f}s")
    plt.title("Dynamic Pressure Profile")
    plt.xlabel("Time (s)")
    plt.ylabel("Dynamic Pressure (kPa)")
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig("docs/figures/fig3_qdyn.png")
    plt.close()

    print("Successfully generated high-resolution academic graphs in docs/figures/")

if __name__ == "__main__":
    generate_graphs()
