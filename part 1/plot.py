import matplotlib.pyplot as plt
import json
import subprocess
import time

def run_experiment(p_values, runs=10):
    results = []
    for p in p_values:
        total_time = 0
        for _ in range(runs):
            # Update config.json with current p value
            with open('config.json', 'r+') as f:
                config = json.load(f)
                config['p'] = p
                f.seek(0)
                json.dump(config, f, indent=4)
                f.truncate()

            # Run the client and measure time
            start_time = time.time()
            subprocess.run(['./client', 'config.json'], stdout=subprocess.DEVNULL)
            end_time = time.time()
            total_time += (end_time - start_time)

        avg_time = total_time / runs
        results.append(avg_time)

    return results

p_values = range(1, 11)
completion_times = run_experiment(p_values)

plt.errorbar(p_values, completion_times, yerr=None, capsize=5)
plt.xlabel('p (words per packet)')
plt.ylabel('Average Completion Time (s)')
plt.title('Completion Time vs Words per Packet')
plt.savefig('plot.png')
plt.close()