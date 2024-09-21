import matplotlib.pyplot as plt
import json
import subprocess
import time
import numpy as np


def run_experiment(client_numbers, runs=5):
    results = {}
    for num_clients in client_numbers:
        run_times = []
        for _ in range(runs):
            with open("config.json", "r+") as f:
                config = json.load(f)
                config["num_clients"] = num_clients
                f.seek(0)
                json.dump(config, f, indent=4)
                f.truncate()

            start_time = time.time()
            process = subprocess.Popen(
                ["./client", "config.json"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
            output, _ = process.communicate()
            print(output)
            end_time = time.time()

            for line in output.split("\n"):
                if "Average time per client:" in line:
                    avg_time_per_client = float(line.split(":")[1].strip().split()[0])
                    run_times.append(avg_time_per_client)
                    break

        avg_time = np.mean(run_times)
        std_time = np.std(run_times)
        std_error = std_time / np.sqrt(runs)
        print(
            f"Clients: {num_clients}, average time per client: {avg_time:.4f}, std: {std_time:.4f}, stderr: {std_error:.4f}"
        )
        results[num_clients] = (avg_time, std_error)

    return results


client_numbers = [1, 5, 9, 13, 17, 21, 25, 29, 32]
results = run_experiment(client_numbers)

# Prepare data for plotting
clients = list(results.keys())
avg_times = [result[0] for result in results.values()]
std_errors = [result[1] for result in results.values()]

# Create the plot
plt.figure(figsize=(12, 6))
plt.errorbar(
    clients, avg_times, yerr=std_errors, capsize=5, fmt="o-", ecolor="r", elinewidth=2
)
plt.xlabel("Number of Clients")
plt.ylabel("Average Completion Time per Client (s)")
plt.title("Average Completion Time per Client vs Number of Clients")
plt.grid(True)
plt.xticks(clients)

plt.savefig("plot.png")

print("Plot saved as multi_client_plot.png")
