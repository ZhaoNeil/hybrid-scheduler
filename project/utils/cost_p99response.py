from response_time import time_measure
import matplotlib.pyplot as plt
from adjustText import adjust_text
import numpy as np


fifo_res, fifo_turn, fifo_exec = time_measure("project/log/metrics_FIFO.txt")
cfs_res, cfs_turn, cfs_exec = time_measure("project/log/metrics_CFS.txt")
edf_res, edf_turn, edf_exec = time_measure("project/log/metrics_EDF.txt")
sol_res, sol_turn, sol_exec = time_measure("project/log/metrics_sol.txt")
our_res, our_turn, our_exec = time_measure("project/log/metrics_Hybrid_1633ms_25.txt")

portion = [
    0.1172747,
    0.83123968,
    0.03785329,
    0.01363232,
]

mem_size = [128, 512, 1024, 1536]

# price for 1ms from https://aws.amazon.com/lambda/pricing/
price = [
    0.0000000021,
    0.0000000083,
    0.0000000167,
    0.0000000250,
]

price_expectation = sum(x * y for x, y in zip(portion, price)) * 1000

fifo_cost = sum(fifo_exec) * price_expectation
cfs_cost = sum(cfs_exec) * price_expectation
edf_cost = sum(edf_exec) * price_expectation
sol_cost = sum(sol_exec) * price_expectation
our_cost = sum(our_exec) * price_expectation


def percentile(data, percentile):
    size = len(data)
    return sorted(data)[int(size * percentile)]


fifo_res_p99 = percentile(fifo_res, 0.99)
cfs_res_p99 = percentile(cfs_res, 0.99)
edf_res_p99 = percentile(edf_res, 0.99)
sol_res_p99 = percentile(sol_res, 0.99)
our_res_p99 = percentile(our_res, 0.99)


costs = [fifo_cost, cfs_cost, edf_cost, sol_cost]
p99_res_times = [
    fifo_res_p99,
    cfs_res_p99,
    edf_res_p99,
    sol_res_p99,
]
labels = ["FIFO", "CFS", "EDF", "SOL"]

plt.figure(figsize=(9, 4.5))
plt.scatter(costs, p99_res_times, color="lightcoral", s=150)
plt.scatter(
    [our_cost], [our_res_p99], color="cornflowerblue", s=150, label="Our Scheduler"
)

plt.annotate(
    "FIFO",
    (fifo_cost, fifo_res_p99),
    textcoords="offset points",
    xytext=(5, -25),
    ha="center",
    size=18,
)
plt.annotate(
    "CFS",
    (cfs_cost, cfs_res_p99),
    textcoords="offset points",
    xytext=(0, 10),
    ha="center",
    size=18,
)
plt.annotate(
    "EDF",
    (edf_cost, edf_res_p99),
    textcoords="offset points",
    xytext=(25, -5),
    ha="center",
    size=18,
)
plt.annotate(
    "SOL",
    (sol_cost, sol_res_p99),
    textcoords="offset points",
    xytext=(-25, -5),
    ha="center",
    size=18,
)
plt.annotate(
    "Our Scheduler",
    (our_cost, our_res_p99),
    textcoords="offset points",
    xytext=(30, 10),
    ha="center",
    size=18,
)


plt.xlabel("Overall Cost ($)", size=23)
plt.ylabel("p99 Response Time (ms)", size=22)
plt.xticks(size=23)
plt.yticks(size=23)
plt.grid(True, linestyle="--")
plt.tight_layout()
plt.savefig("project/log/cost_p99response.png")
