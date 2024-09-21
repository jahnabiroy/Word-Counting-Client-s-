import subprocess
import json
import numpy as np
import matplotlib.pyplot as plt
import os
import time


def run_server(scheduling_policy):
    try:
        return subprocess.Popen(["./server", scheduling_policy])
    except FileNotFoundError:
        print(
            "Error: Server executable not found. Make sure you've compiled the server."
        )
        return None


def run_client():
    try:
        return subprocess.Popen(["./client"])
    except FileNotFoundError:
        print(
            "Error: Client executable not found. Make sure you've compiled the client."
        )
        return None


def parse_client_output(client_id):
    filename = f"output_client_{client_id}.txt"
    try:
        with open(filename, "r") as f:
            lines = f.readlines()
            if lines:
                last_line = lines[-1].strip().split(", ")
                if len(last_line) >= 2:
                    return float(last_line[1])
    except FileNotFoundError:
        print(f"Warning: Output file {filename} not found.")
    return None


def jains_fairness_index(x):
    return (np.sum(x) ** 2) / (len(x) * np.sum(x**2))


def run_experiment(scheduling_policy):
    server_process = run_server(scheduling_policy)
    if server_process is None:
        return None, None, None

    time.sleep(1)  # Give the server time to start

    # Set up config for 1 client (we'll run multiple instances)
    try:
        with open("config.json", "r") as f:
            config = json.load(f)
        config["num_clients"] = 1
        with open("config.json", "w") as f:
            json.dump(config, f)
    except FileNotFoundError:
        print("Error: config.json not found. Make sure it's in the same directory.")
        return None, None, None

    # Run 9 normal clients
    for _ in range(9):
        if run_client() is None:
            return None, None, None

    # Run 1 rogue client (5 concurrent requests)
    for _ in range(5):
        if run_client() is None:
            return None, None, None

    # Wait for all clients to finish
    time.sleep(60)  # Adjust this value if needed

    server_process.terminate()
    time.sleep(1)  # Give the server time to shut down

    # Parse output files
    # Parse output files
    client_times = []
    for i in range(14):  # 9 normal + 5 rogue = 14 total
        client_time = parse_client_output(i)  # Renamed variable
        if client_time is not None:
            client_times.append(client_time)
        else:
            print(f"Warning: No valid time found for client {i}.")

    if not client_times:
        print("Error: No valid client times found.")
        return None, None, None

    avg_time = np.mean(client_times)
    fairness_index = jains_fairness_index(np.array(client_times))

    return avg_time, fairness_index, client_times


def plot_results(fifo_times, fair_times):
    if not fifo_times or not fair_times:
        print("Error: Not enough data to plot results.")
        return

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 6))

    ax1.boxplot([fifo_times, fair_times], labels=["FIFO", "Fair"])
    ax1.set_ylabel("Completion Time (seconds)")
    ax1.set_title("Distribution of Completion Times")

    ax2.bar(["FIFO", "Fair"], [np.mean(fifo_times), np.mean(fair_times)])
    ax2.set_ylabel("Average Completion Time (seconds)")
    ax2.set_title("Average Completion Time Comparison")

    plt.tight_layout()
    plt.savefig("rogue_client_experiment_results.png")
    plt.show()


def main():
    scheduling_policies = ["fifo", "fair"]
    results = {}

    for policy in scheduling_policies:
        print(f"Running experiment with {policy.upper()} scheduling...")
        avg_time, fairness_index, client_times = run_experiment(policy)
        if avg_time is not None:
            results[policy] = {
                "avg_time": avg_time,
                "fairness_index": fairness_index,
                "client_times": client_times,
            }
        else:
            print(f"Error occurred during {policy.upper()} experiment.")

    if not results:
        print("No valid results to report.")
        return

    print("\nExperiment Results:")
    for policy, data in results.items():
        print(f"\n{policy.upper()} scheduling:")
        print(f"  Average completion time: {data['avg_time']:.2f} seconds")
        print(f"  Jain's fairness index: {data['fairness_index']:.4f}")

    if "fifo" in results and "fair" in results:
        plot_results(results["fifo"]["client_times"], results["fair"]["client_times"])
    else:
        print("Not enough data to plot results.")


if __name__ == "__main__":
    main()
