import matplotlib.pyplot as plt
import json
import subprocess
import numpy as np

def run_experiment(client_counts, runs=10):
    results = []
    for n in client_counts:
        times = []
        for _ in range(runs):
            # Update config.json with current n value
            with open('config.json', 'r+') as f:
                config = json.load(f)
                config['n'] = n
                f.seek(0)
                json.dump(config, f, indent=4)
                f.truncate()

            # Run the client and capture output
            output = subprocess.check_output(['./client', 'config.json', str(n)], universal_newlines=True)
            avg_time = float(output.split()[-2])  # Extract the average time from the output
            times.append(avg_time)

        avg_time = np.mean(times)
        std_dev = np.std(times)
        results.append((avg_time, std_dev))

    return results

client_counts = range(1, 33, 4)
results = run_experiment(client_counts)

avg_times, std_devs = zip(*results)

plt.errorbar(client_counts, avg_times, yerr=std_devs, capsize=5)
plt.xlabel('Number of Concurrent Clients')
plt.ylabel('Average Completion Time per Client (s)')
plt.title('Completion Time vs Number of Concurrent Clients')
plt.savefig('plot.png')
plt.close()