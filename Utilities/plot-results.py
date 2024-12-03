import json
import subprocess
import numpy as np
import matplotlib.pyplot as plt
from scipy import stats

def run_experiment(p):
    with open('config.json', 'r') as f:
        config = json.load(f)
    
    config['p'] = p
    
    with open('config.json', 'w') as f:
        json.dump(config, f)
    
    times = []
    for _ in range(10):
        output = subprocess.check_output(['./client'], universal_newlines=True)
        time = float(output.split()[-2])
        times.append(time)
    
    return np.mean(times), stats.sem(times)

def main():
    p_values = range(1, 11)
    means = []
    errors = []

    for p in p_values:
        mean, error = run_experiment(p)
        means.append(mean)
        errors.append(error)

    plt.errorbar(p_values, means, yerr=errors, fmt='o-')
    plt.xlabel('p (words per packet)')
    plt.ylabel('Completion Time (seconds)')
    plt.title('Completion Time vs Words per Packet')
    plt.savefig('plot.png')
    plt.close()

if __name__ == '__main__':
    main()
