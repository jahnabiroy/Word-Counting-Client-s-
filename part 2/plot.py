import matplotlib.pyplot as plt
import json
import numpy as np
import sys
import os


def read_config():
    with open("config.json", "r") as f:
        return json.load(f)


def read_client_times(num_clients):
    times = []
    for i in range(num_clients):
        filename = f"output_client_{i}.txt"
        if not os.path.exists(filename):
            print(f"Error: {filename} not found. Make sure to run the client first.")
            sys.exit(1)

        with open(filename, "r") as f:
            # Assuming the last line of each file contains the client's completion time
            for line in f:
                pass
            last_line = line

        # Extract the time from the last line
        time = float(last_line.split()[-2])
        times.append(time)

    return times


def plot_results(client_numbers, avg_times, std_dev):
    plt.figure(figsize=(10, 6))
    plt.errorbar(client_numbers, avg_times, yerr=std_dev, fmt="o-", capsize=5)
    plt.xlabel("Number of Clients")
    plt.ylabel("Average Completion Time per Client (seconds)")
    plt.title("Average Completion Time vs Number of Clients")
    plt.grid(True)
    plt.savefig("plot.png")
    plt.close()


def main():
    config = read_config()
    max_clients = config.get("max_clients", 32)
    client_numbers = list(range(1, max_clients + 1, 4))

    avg_times = []
    std_dev = []

    for num_clients in client_numbers:
        times = read_client_times(num_clients)
        avg_times.append(np.mean(times))
        std_dev.append(np.std(times))

    plot_results(client_numbers, avg_times, std_dev)
    print(f"Plot saved as plot.png")


if __name__ == "__main__":
    main()
