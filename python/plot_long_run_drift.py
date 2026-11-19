"""Plot results/paper_benchmark/long_run_energy_drift_dt*_series.csv -> long_run_energy_drift.png."""
import csv
import collections
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

OUT = 'results/paper_benchmark'
fig, axes = plt.subplots(1, 2, figsize=(11, 4.2), sharey=False)
colors = {'RK4': '#1f77b4', 'Leapfrog': '#ff7f0e', 'Yoshida4': '#2ca02c', 'Hermite': '#9467bd', 'RK45': '#7f7f7f'}
for ax, tag, title in [(axes[0], 'dt3600', 'dt = 1 h'), (axes[1], 'dt86400', 'dt = 1 day')]:
    d = collections.defaultdict(list)
    for r in csv.DictReader(open(f'{OUT}/long_run_energy_drift_{tag}_series.csv')):
        if float(r['time_yr']) < 0.5:
            continue  # first snapshot is the energy reference (zero drift)
        d[r['integrator']].append((float(r['time_yr']), max(float(r['abs_dE_over_E0']), 1e-17)))
    for k, v in d.items():
        ax.loglog(*zip(*v), label=k, color=colors[k.split()[0]], lw=1.2,
                  ls='--' if k.startswith('RK45') else '-')
    ax.set_title(f'10-body Solar System, GR off, {title}')
    ax.set_xlabel('Time (years)')
    ax.grid(True, which='both', alpha=0.3)
axes[0].set_ylabel('|ΔE / E₀|')
axes[0].legend(fontsize=8)
plt.tight_layout()
plt.savefig(f'{OUT}/long_run_energy_drift.png', dpi=150)
print('wrote', f'{OUT}/long_run_energy_drift.png')
