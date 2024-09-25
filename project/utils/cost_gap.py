from response_time import time_measure
import numpy as np
import matplotlib.pyplot as plt

# script for calculating how much cost does CFS increase
# 128MB, 512MB, 1024MB, 1536MB
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

fifo_res, fifo_turn, fifo_exec = time_measure("project/log/metrics_FIFO.txt")
cfs_res, cfs_turn, cfs_exec = time_measure("project/log/metrics_CFS.txt")
our_res, our_turn, our_exec = time_measure("project/log/metrics_Hybrid_1633ms_25.txt")

fifo_avg = sum(fifo_exec) / len(fifo_exec)
cfs_avg = sum(cfs_exec) / len(cfs_exec)
our_avg = sum(our_exec) / len(our_exec)

price_expectation = sum(x * y for x, y in zip(portion, price)) * 1000

print(f"FIFO average execution time: {fifo_avg} s")
print(f"CFS average execution time: {cfs_avg} s")
print(f"Our average execution time: {our_avg} s")
print(f"Price expectation: {price_expectation} usd/s")

print(f"FIFO overall cost: {sum(fifo_exec) * price_expectation} usd")
print(f"CFS overall cost: {sum(cfs_exec) * price_expectation} usd")
print(f"Our overall cost: {sum(our_exec) * price_expectation} usd")

cost_gap = (cfs_avg - fifo_avg) * price_expectation * 12442
print(f"Cost gap: {cost_gap} usd")


# write a function to get the p99 percentile of a list
def percentile(data, percentile):
    size = len(data)
    return sorted(data)[int(size * percentile)]


fifo_res_p99 = percentile(fifo_res, 0.99)
cfs_res_p99 = percentile(cfs_res, 0.99)
our_res_p99 = percentile(our_res, 0.99)
fifo_turn_p99 = percentile(fifo_turn, 0.99)
cfs_turn_p99 = percentile(cfs_turn, 0.99)
our_turn_p99 = percentile(our_turn, 0.99)
fifo_exec_p99 = percentile(fifo_exec, 0.99)
cfs_exec_p99 = percentile(cfs_exec, 0.99)
our_exec_p99 = percentile(our_exec, 0.99)

print(f"FIFO p99 response time: {fifo_res_p99} s")
print(f"CFS p99 response time: {cfs_res_p99} s")
print(f"Our p99 response time: {our_res_p99} s")
print(f"FIFO p99 execution time: {fifo_exec_p99} s")
print(f"CFS p99 execution time: {cfs_exec_p99} s")
print(f"Our p99 execution time: {our_exec_p99} s")
print(f"FIFO p99 turnaround time: {fifo_turn_p99} s")
print(f"CFS p99 turnaround time: {cfs_turn_p99} s")
print(f"Our p99 turnaround time: {our_turn_p99} s")

# price for 1ms from https://aws.amazon.com/lambda/pricing/
full_price_list = [
    0.0000000021,
    0.0000000083,
    0.0000000167,
    0.0000000250,
    0.0000000333,
    0.0000000500,
    0.0000000667,
    0.0000000833,
    0.0000001000,
    0.0000001167,
    0.0000001333,
    0.0000001500,
    0.0000001667,
]

mem_list = [128, 512, 1024, 1536, 2048, 3072, 4096, 5120, 6144, 7168, 8192, 9216, 10240]

fifo_cost = [sum(fifo_exec) * i * 1000 for i in full_price_list]
cfs_cost = [sum(cfs_exec) * i * 1000 for i in full_price_list]
our_cost = [sum(our_exec) * i * 1000 for i in full_price_list]

# print histogram plot for fifo and cfs cost
index = range(len(mem_list))

plt.figure(figsize=(9, 4.5))
plt.bar(index, fifo_cost, width=0.4, color="cornflowerblue", label="FIFO Cost")
plt.bar(
    [p + 0.4 for p in index], cfs_cost, width=0.4, color="lightcoral", label="CFS Cost"
)
plt.xlabel("Function Memory Size (MB)", size=23)
plt.ylabel("Cost (USD)", size=23)
plt.yticks(size=20)
plt.xticks(
    [p + 0.2 for p in index],
    labels=[str(mem_list[i]) for i in range(len(mem_list))],
    size=20,
    rotation=45,
)
plt.legend(fontsize=20)
plt.tight_layout()
plt.savefig("project/log/cost_gap_fifo_cfs.png")


bar_width = 0.3  # Adjust the width of each bar

plt.figure(figsize=(9, 4.5))
plt.bar(
    [p - bar_width for p in index],
    our_cost,
    width=bar_width,
    color="limegreen",
    label="Our Scheduler Cost",
)
plt.bar(
    index,
    fifo_cost,
    width=bar_width,
    color="cornflowerblue",
    label="FIFO Cost",
)
plt.bar(
    [p + bar_width for p in index],
    cfs_cost,
    width=bar_width,
    color="lightcoral",
    label="CFS Cost",
)
plt.xlabel("Function Memory Size (MB)", size=23)
plt.ylabel("Cost (USD)", size=23)
plt.yticks(size=20)
plt.xticks(
    index,
    labels=[str(mem_list[i]) for i in range(len(mem_list))],
    size=20,
    rotation=45,
)
plt.legend(fontsize=20)
plt.tight_layout()
plt.savefig("project/log/cost_gap_our.png")


cfs_firecracker_exec = time_measure("project/log/metrics_CFS_firecracker_2952.txt")[2]
our_firecracker_exec = time_measure("project/log/metrics_Hybrid_firecracker_2952.txt")[
    2
]

cfs_firecracker_cost = [sum(cfs_firecracker_exec) * i * 1000 for i in full_price_list]
our_firecracker_cost = [sum(our_firecracker_exec) * i * 1000 for i in full_price_list]

plt.figure(figsize=(9, 4.5))
plt.bar(
    [p - 0.4 for p in index],
    our_firecracker_cost,
    width=0.4,
    color="cornflowerblue",
    label="Our Scheduler Cost",
)
plt.bar(
    index,
    cfs_firecracker_cost,
    width=0.4,
    color="lightcoral",
    label="CFS Cost",
)

plt.xlabel("Function Memory Size (MB)", size=23)
plt.ylabel("Cost (USD)", size=23)
plt.yticks(size=20)
plt.xticks(
    [p for p in index],
    labels=[str(mem_list[i]) for i in range(len(mem_list))],
    size=20,
    rotation=45,
)
plt.legend(fontsize=20)
plt.tight_layout()
plt.savefig("project/log/cost_gap_firecracker.png")


cfs_firecracker_overall = sum(cfs_firecracker_exec) * price_expectation
our_firecracker_overall = sum(our_firecracker_exec) * price_expectation

print(f"CFS Firecracker overall cost: {cfs_firecracker_overall} usd")
print(f"Our Firecracker overall cost: {our_firecracker_overall} usd")
