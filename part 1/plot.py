import matplotlib.pyplot as plt
import json
import subprocess
import time
import numpy as np


def run_experiment(p_values, runs=10):
    results = []
    for p in p_values:
        run_times = []
        for _ in range(runs):
            with open("config.json", "r+") as f:
                config = json.load(f)
                config["p"] = p
                f.seek(0)
                json.dump(config, f, indent=4)
                f.truncate()

            start_time = time.time()
            subprocess.run(["./client", "config.json"], stdout=subprocess.DEVNULL)
            end_time = time.time()
            run_times.append(end_time - start_time)

        avg_time = np.mean(run_times)
        std_time = np.std(run_times)
        std_error = std_time / np.sqrt(runs)
        print(
            f"p-value: {p}, average time: {avg_time:.4f}, std: {std_time:.4f}, stderr: {std_error:.4f}"
        )
        results.append((avg_time, std_error))

    return results


p_values = range(1, 11)
completion_times_with_error = run_experiment(p_values)

completion_times = [result[0] for result in completion_times_with_error]
std_errors = [result[1] for result in completion_times_with_error]

plt.errorbar(
    p_values,
    completion_times,
    yerr=std_errors,
    capsize=5,
    fmt="o-",
    ecolor="r",
    elinewidth=2,
)
plt.xlabel("p (words per packet)")
plt.ylabel("Average Completion Time (s)")
plt.title("Completion Time vs Words per Packet (with Confidence Intervals)")
plt.savefig("plot.png")
plt.show()
