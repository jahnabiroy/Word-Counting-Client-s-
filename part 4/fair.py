import matplotlib.pyplot as plt
import json
import subprocess
import time
import numpy as np
from concurrent.futures import ThreadPoolExecutor, as_completed


def run_client(config_file):
    process = subprocess.Popen(
        ["./client", config_file],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    output, _ = process.communicate()
    for line in output.split("\n"):
        if "Average time per client:" in line:
            return float(line.split(":")[1].strip().split()[0])
    return None


def run_rogue_client_experiment(scheduling_policy):
    with open("config_4.json", "r+") as f:
        config = json.load(f)
        config["num_clients"] = 3
        f.seek(0)
        json.dump(config, f, indent=4)
        f.truncate()

    server_process = subprocess.Popen(["./server", scheduling_policy])
    time.sleep(1)  # Give the server time to start

    def run_normal_client():
        return run_client("config_4.json")

    def run_rogue_client():
        times = []
        for _ in range(5):
            times.append(run_client("config_4.json"))
        return np.mean(times)

    with ThreadPoolExecutor(max_workers=10) as executor:
        futures = [executor.submit(run_rogue_client)] + [
            executor.submit(run_normal_client) for _ in range(9)
        ]
        client_times = [
            future.result()
            for future in as_completed(futures)
            if future.result() is not None
        ]

    server_process.terminate()
    server_process.wait()

    return client_times


def calculate_jains_fairness(times):
    n = len(times)
    return (np.sum(times) ** 2) / (n * np.sum(np.square(times)))


fair_rogue_times = run_rogue_client_experiment("fair")
fifo_rogue_times = run_rogue_client_experiment("fifo")

fair_avg_time = np.mean(fair_rogue_times)
fair_jains_index = calculate_jains_fairness(fair_rogue_times)

fifo_avg_time = np.mean(fifo_rogue_times)
fifo_jains_index = calculate_jains_fairness(fifo_rogue_times)

print("Rogue Client Scenario Results:")
print(
    f"Fair Scheduling - Avg Time: {fair_avg_time:.4f}, Jain's Fairness Index: {fair_jains_index:.4f}"
)
print(
    f"FIFO Scheduling - Avg Time: {fifo_avg_time:.4f}, Jain's Fairness Index: {fifo_jains_index:.4f}"
)

with open("fairness.txt", "w") as f:
    f.write("\nRogue Client Scenario Results:\n")
    f.write(
        f"Fair Scheduling - Avg Time: {fair_avg_time:.4f}, Jain's Fairness Index: {fair_jains_index:.4f}\n"
    )
    f.write(
        f"FIFO Scheduling - Avg Time: {fifo_avg_time:.4f}, Jain's Fairness Index: {fifo_jains_index:.4f}\n"
    )

print("Analysis complete. Results saved in 'fairness.txt'")
