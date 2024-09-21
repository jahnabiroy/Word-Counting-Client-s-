import subprocess
import time
import json
import matplotlib.pyplot as plt


def run_server(scheduling_policy):
    return subprocess.Popen(["./server", scheduling_policy])


def run_client(num_clients):
    with open("config.json", "r") as f:
        config = json.load(f)
    config["num_clients"] = num_clients
    with open("config.json", "w") as f:
        json.dump(config, f)
    return subprocess.run(["./client"], capture_output=True, text=True)


def parse_client_output(output):
    lines = output.stdout.split("\n")
    for line in reversed(lines):
        if "Average time per client:" in line:
            return float(line.split(": ")[1].split(" ")[0])
    print("WARNING: Could not find average time in output:")
    print(output.stdout)
    return None


def run_experiment(n_values, scheduling_policies):
    results = {policy: [] for policy in scheduling_policies}

    for policy in scheduling_policies:
        for n in n_values:
            print(f"Running experiment: {policy} scheduling with {n} clients")
            server_process = run_server(policy)
            time.sleep(1)  # Give the server time to start

            client_output = run_client(n)
            server_process.terminate()
            time.sleep(1)  # Give the server time to shut down

            avg_time = parse_client_output(client_output)
            if avg_time is not None:
                results[policy].append(avg_time)
                print(
                    f"Completed: {policy} scheduling with {n} clients. Average time: {avg_time:.2f} seconds"
                )
            else:
                print(
                    f"ERROR: Could not get average time for {policy} scheduling with {n} clients"
                )
                results[policy].append(None)

    return results


def plot_results(n_values, results):
    plt.figure(figsize=(10, 6))
    for policy, times in results.items():
        valid_times = [(n, t) for n, t in zip(n_values, times) if t is not None]
        if valid_times:
            n_valid, times_valid = zip(*valid_times)
            plt.plot(n_valid, times_valid, marker="o", label=policy)
    plt.xlabel("Number of Clients (n)")
    plt.ylabel("Average Completion Time (seconds)")
    plt.title("Performance Comparison of Scheduling Policies")
    plt.legend()
    plt.grid(True)
    plt.savefig("scheduling_performance.png")
    plt.show()


def main():
    n_values = [1, 2, 4, 8, 16, 32]
    scheduling_policies = ["fifo", "fair"]

    results = run_experiment(n_values, scheduling_policies)
    plot_results(n_values, results)

    print("\nExperiment Results:")
    for policy in scheduling_policies:
        print(f"\n{policy.upper()} scheduling:")
        for n, time in zip(n_values, results[policy]):
            if time is not None:
                print(f"  n = {n}: {time:.2f} seconds")
            else:
                print(f"  n = {n}: Error occurred")


if __name__ == "__main__":
    main()
