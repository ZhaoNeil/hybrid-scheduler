import json
import matplotlib.pyplot as plt
import ast
import numpy as np
from response_time import time_measure
from preemption_count import preemption_count, read_values_after_last_preemption


# plot the time elapsed for different schedulers
def plot_schedulers():
    schedulers = []
    time_elapsed = []

    with open("project/log/scheduler_results.txt", "r") as f:
        lines = f.readlines()
        lines = lines[1:]  # read lines from the second line
        for line in lines:
            scheduler = line.split(" ")[0]
            time = round(float(line.split(" ")[-2]))
            schedulers.append(scheduler)
            time_elapsed.append(time)

    plt.figure(figsize=(8, 6))
    plt.bar(schedulers, time_elapsed, color="cornflowerblue")
    plt.xlabel("Schedulers", size=23)
    plt.ylabel("Time Elapsed (s)", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig("project/log/time_schedulers.png")


# plot the time elapsed for different number of CPUs
def plot_throughput():
    cpus = []
    time_elapsed = []

    with open("project/log/throughput_results.txt", "r") as f:
        lines = f.readlines()
        lines = lines[1:]
        for line in lines:
            cpu = line.split(" ")[1]
            time = round(float(line.split(" ")[-2]))
            cpus.append(cpu)
            time_elapsed.append(time)

    plt.figure(figsize=(8, 6))
    plt.bar(cpus, time_elapsed, color="cornflowerblue")
    plt.xlabel("Range of CPUs", size=23)
    plt.ylabel("Time Elapsed (s)", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    # plt.xticks(rotation=45)
    plt.tight_layout()
    plt.savefig("project/log/time_cpus.png")


# plot the average CPU utilization for short list and long list cpu groups
def plot_utilization(sched_1, sched_2, sched_1_line, sched_2_line):
    with open("project/log/utilization.txt", "r") as f:
        lines = f.readlines()
        # Convert the string to list using ast.literal_eval()
        short_avg = ast.literal_eval(lines[sched_1_line].split(": ")[1])
        long_avg = ast.literal_eval(lines[sched_2_line].split(": ")[1])

        # Plot short_avg and long_avg separately
        plt.figure(figsize=(9, 4.5))
        plt.plot(
            range(len(short_avg)),
            short_avg,
            label=f"{sched_1} Cores (0-24)",
            color="cornflowerblue",
            linewidth="3.0",
        )
        plt.plot(
            range(len(long_avg)),
            long_avg,
            label=f"{sched_2} Cores (25-49)",
            color="lightcoral",
            linewidth="3.0",
        )

        # Set labels and legends
        plt.xlabel("Time (Seconds)", size=23)
        plt.ylabel("Avg CPU Utilization (%)", size=23)
        plt.xticks(size=20)
        plt.yticks(size=20)
        plt.ylim(0, 105)
        plt.xlim(-5, max(len(short_avg), len(long_avg)) + 5)
        plt.legend(fontsize=20)
        plt.tight_layout()

        # Save the plot
        plt.savefig(f"project/log/cpu_utilization_{sched_1}+{sched_2}.png")


# plot the average CPU utilization for short list and long list cpu groups and preemption time slice
def plot_utilization_preemption(
    sched_1, sched_2, sched_1_line, sched_2_line, preempt_file
):
    with open("project/log/utilization.txt", "r") as f:
        lines = f.readlines()
        short_avg = ast.literal_eval(lines[sched_1_line].split(": ")[1])
        long_avg = ast.literal_eval(lines[sched_2_line].split(": ")[1])
        preemption_time_slice = read_values_after_last_preemption(preempt_file)

        gap = len(preemption_time_slice) - len(short_avg)
        # print(len(preemption_time_slice))
        # print(len(short_avg))

        plt.figure(figsize=(9, 4.5))
        ax1 = plt.gca()
        ax1.plot(
            range(600),
            short_avg[:600],
            label=f"{sched_1} Cores (0-24)",
            color="cornflowerblue",
            linewidth="3.0",
        )
        ax1.plot(
            range(600),
            long_avg[:600],
            label=f"{sched_2} Cores (25-49)",
            color="lightcoral",
            linewidth="3.0",
        )

        ax1.set_xlabel("Time (Seconds)", size=23)
        ax1.set_ylabel("Avg CPU Utilization (%)", size=23)
        ax1.set_ylim(0, 105)
        # ax1.set_xlim(-5, max(len(short_avg), len(long_avg)) + 5)
        ax1.set_xlim(-5, 600)
        ax1.tick_params(axis="both", which="major", labelsize=20)

        ax2 = ax1.twinx()
        shifted_time = [x - gap for x in range(len(preemption_time_slice))]
        ax2.plot(
            range(600),
            preemption_time_slice[0:600],
            label="Time Limit",
            color="limegreen",
            linewidth="3.0",
            linestyle="--",
        )
        ax2.set_ylabel("Time Limit (s)", size=23)
        ax2.tick_params(axis="y", labelsize=20)

        lines, labels = ax1.get_legend_handles_labels()
        lines2, labels2 = ax2.get_legend_handles_labels()
        ax2.legend(lines + lines2, labels + labels2, fontsize=20, loc="lower right")

        plt.tight_layout()
        plt.savefig(f"project/log/cpu_utilization_ts_{sched_1}+{sched_2}.png")


# plot the average CPU utilization for short list and long list cpu groups and cpu number
def plot_utilization_cpu_number(sched_1, sched_2):
    with open("project/log/log_cpu_list_adapt.txt", "r") as f:
        numbers = [int(line.strip()) for line in f]

    all_lists = []
    with open("project/log/utilization_adapt_CPU.txt", "r") as f:
        for line in f:
            current_list = ast.literal_eval(line.strip())
            all_lists.append(current_list)

    short_avg = []
    long_avg = []
    for i in range(min(len(all_lists), len(numbers))):
        short_num = numbers[i]
        long_num = 50 - short_num
        short_avg.append(sum(all_lists[i][:short_num]) / short_num)
        long_avg.append(sum(all_lists[i][short_num:]) / long_num)

    plt.figure(figsize=(9, 4.5))
    ax1 = plt.gca()
    ax1.plot(
        range(len(short_avg)),
        short_avg,
        label=f"{sched_1} Cores",
        color="cornflowerblue",
        linewidth="2.0",
    )
    ax1.plot(
        range(len(long_avg)),
        long_avg,
        label=f"{sched_2} Cores",
        color="lightcoral",
        linewidth="2.0",
    )

    ax1.set_xlabel("Time (Seconds)", size=23)
    ax1.set_ylabel("Avg CPU Utilization (%)", size=23)
    ax1.set_ylim(0, 105)
    ax1.set_xlim(-5, max(len(short_avg), len(long_avg)) + 5)
    ax1.tick_params(axis="both", which="major", labelsize=20)

    ax2 = ax1.twinx()
    ax2.plot(
        range(len(short_avg)),
        numbers[: len(short_avg)],
        label="FIFO cores number",
        color="limegreen",
        linewidth="2.0",
        linestyle="--",
    )
    ax2.set_ylabel("Number of FIFO Cores", size=23)
    ax2.tick_params(axis="y", labelsize=20)

    lines, labels = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax2.legend(lines + lines2, labels + labels2, fontsize=20)

    plt.tight_layout()
    plt.savefig(f"project/log/cpu_util_cpu_number_{sched_1}_{sched_2}.png")


# plot the average CPU utilization for short list and long list cpu groups and preemption time slice, cpu number
def plot_utilization_adaptation(sched_1, sched_2):
    with open("project/log/log_cpu_list_adapt_p95.txt", "r") as f:
        numbers = [int(line.strip()) for line in f]

    all_lists = []
    with open("project/log/utilization_adapt_p95.txt", "r") as f:
        for line in f:
            current_list = ast.literal_eval(line.strip())
            all_lists.append(current_list)

    short_avg = []
    long_avg = []
    for i in range(min(len(all_lists), len(numbers))):
        short_num = numbers[i]
        long_num = 50 - short_num
        short_avg.append(sum(all_lists[i][:short_num]) / short_num)
        long_avg.append(sum(all_lists[i][short_num:]) / long_num)

    plt.figure(figsize=(9, 4.5))
    ax1 = plt.gca()
    ax1.plot(
        range(600),
        short_avg[:600],
        label=f"{sched_1} Cores",
        color="cornflowerblue",
        linewidth="2.0",
    )
    ax1.plot(
        range(600),
        long_avg[:600],
        label=f"{sched_2} Cores",
        color="lightcoral",
        linewidth="2.0",
    )

    ax1.set_xlabel("Time (Seconds)", size=23)
    ax1.set_ylabel("Avg CPU Utilization (%)", size=23)
    ax1.set_ylim(0, 105)
    # ax1.set_xlim(-5, max(len(short_avg), len(long_avg)) + 5)
    ax1.set_xlim(-5, 600)
    ax1.tick_params(axis="both", which="major", labelsize=20)

    ax2 = ax1.twinx()
    ax2.plot(
        range(len(short_avg)),
        numbers[: len(short_avg)],
        label="FIFO cores number",
        color="limegreen",
        linewidth="2.0",
        linestyle="--",
    )
    ax2.set_ylabel("Number of FIFO Cores", size=23)
    ax2.tick_params(axis="y", labelsize=20)

    lines, labels = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax2.legend(lines + lines2, labels + labels2, fontsize=20)

    plt.tight_layout()
    plt.savefig(f"project/log/cpu_util_adaptation_{sched_1}_{sched_2}.png")

    plt.figure(figsize=(9, 4.5))
    ax1 = plt.gca()
    ax1.plot(
        range(600),
        short_avg[:600],
        label=f"{sched_1} Cores",
        color="cornflowerblue",
        linewidth="2.0",
    )
    ax1.plot(
        range(600),
        long_avg[:600],
        label=f"{sched_2} Cores",
        color="lightcoral",
        linewidth="2.0",
    )

    ax1.set_xlabel("Time (Seconds)", size=23)
    ax1.set_ylabel("Avg CPU Utilization (%)", size=23)
    ax1.set_ylim(0, 105)
    # ax1.set_xlim(-5, max(len(short_avg), len(long_avg)) + 5)
    ax1.set_xlim(-5, 600)
    ax1.tick_params(axis="both", which="major", labelsize=20)

    ax2 = ax1.twinx()
    preemption_time_slice = read_values_after_last_preemption(
        "preempt_count_SCFS_adapt_p95.txt"
    )
    ax2.plot(
        range(len(preemption_time_slice)),
        preemption_time_slice,
        label="Preemption Time Slice",
        color="limegreen",
        linewidth="2.0",
        linestyle="--",
    )
    ax2.set_ylabel("Preem. Time Slice (s)", size=23)
    ax2.tick_params(axis="y", labelsize=20)

    lines, labels = ax1.get_legend_handles_labels()
    lines2, labels2 = ax2.get_legend_handles_labels()
    ax2.legend(lines + lines2, labels + labels2, fontsize=20)

    plt.tight_layout()
    plt.savefig(f"project/log/cpu_util_adaptation_2_{sched_1}_{sched_2}.png")


# cdf function for a list of data
def plot_cdf(data, label, color, linestyle="solid", linewidth="3.0"):
    sorted_data = np.sort(data)
    yvals = np.arange(len(sorted_data)) / float(len(sorted_data))
    plt.plot(
        sorted_data,
        yvals,
        label=label,
        color=color,
        linestyle=linestyle,
        linewidth=linewidth,
    )


# plot comparison of CDF in response time, turnaround time, and execution time
def plot_cdf_compare(sched_1, sched_2):
    sched_1_response, sched_1_turnaround, sched_1_execution = time_measure(
        f"project/log/metrics_{sched_1}.txt"
    )
    sched_2_response, sched_2_turnaround, sched_2_execution = time_measure(
        f"project/log/metrics_{sched_2}.txt"
    )

    # plot the CDF of response time
    plt.figure(figsize=(9, 4.5))
    plot_cdf(sched_1_response, f"{sched_1}", "cornflowerblue")
    plot_cdf(sched_2_response, f"{sched_2}", "lightcoral")

    plt.xlabel("Response Time (seconds)", size=23)
    plt.ylabel("Cumulative prob", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    # plt.title(f"Response Time of {sched_1} and {sched_2}", size=22)
    plt.legend(fontsize=20)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(f"project/log/{sched_1}+{sched_2}_response.png")

    # plot the CDF of turnaround time
    plt.figure(figsize=(9, 4.5))
    plot_cdf(sched_1_turnaround, f"{sched_1}", "cornflowerblue")
    plot_cdf(sched_2_turnaround, f"{sched_2}", "lightcoral")

    plt.xlabel("Turnaround Time (seconds)", size=23)
    plt.ylabel("Cumulative prob", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    # plt.title(f"Turnaround Time of {sched_1} and {sched_2}", size=22)
    plt.legend(fontsize=20)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(f"project/log/{sched_1}+{sched_2}_turnaround.png")

    # plot the CDF of execution time
    plt.figure(figsize=(9, 4.5))
    plot_cdf(sched_1_execution, f"{sched_1}", "cornflowerblue")
    plot_cdf(sched_2_execution, f"{sched_2}", "lightcoral")

    plt.xlabel("Execution Time (seconds)", size=23)
    plt.ylabel("Cumulative prob", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    # plt.title(f"Execution Time of {sched_1} and {sched_2}", size=22)
    plt.legend(fontsize=20)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(f"project/log/{sched_1}+{sched_2}_execution.png")


def plot_cdf_compare_assemble(sched_1, sched_2):
    sched_1_response, sched_1_turnaround, sched_1_execution = time_measure(
        f"project/log/metrics_{sched_1}.txt"
    )
    sched_2_response, sched_2_turnaround, sched_2_execution = time_measure(
        f"project/log/metrics_{sched_2}.txt"
    )

    plt.figure(figsize=(9, 4.5))  # Adjust figure size for clarity

    # Plotting all metrics in one plot with different line styles
    plot_cdf(
        sched_1_execution, f"{sched_1} Execution", "cornflowerblue", linestyle="dotted"
    )
    plot_cdf(
        sched_1_response, f"{sched_1} Response", "cornflowerblue", linestyle="dashed"
    )
    plot_cdf(
        sched_1_turnaround,
        f"{sched_1} Turnaround",
        "cornflowerblue",
        linestyle="solid",
    )
    plot_cdf(
        sched_2_execution, f"{sched_2} Execution", "lightcoral", linestyle="dotted"
    )
    plot_cdf(sched_2_response, f"{sched_2} Response", "lightcoral", linestyle="dashed")
    plot_cdf(
        sched_2_turnaround, f"{sched_2} Turnaround", "lightcoral", linestyle="solid"
    )

    plt.xlabel("Time (seconds)", size=23)
    plt.ylabel("Cumulative prob", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    plt.legend(fontsize=20)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(f"project/log/{sched_1}+{sched_2}_assemble.png")


def plot_cdf_compare_subplot(sched_1, sched_2, label_1, label_2):
    sched_1_response, sched_1_turnaround, sched_1_execution = time_measure(
        f"project/log/metrics_{sched_1}.txt"
    )
    sched_2_response, sched_2_turnaround, sched_2_execution = time_measure(
        f"project/log/metrics_{sched_2}.txt"
    )

    plt.figure(figsize=(18, 5))  # Adjusted for better fit of three plots

    # Execution Time Plot
    plt.subplot(131)  # 3 rows, 1 column, 1st subplot
    plot_cdf(sched_1_execution, label_1, "cornflowerblue", linestyle="solid")
    plot_cdf(sched_2_execution, label_2, "lightcoral", linestyle="solid")
    plt.xlabel("Execution Time (s)", size=35)
    plt.ylabel("Cumulative prob", size=33)
    plt.xticks(np.arange(0, 20, step=4), size=35)
    plt.yticks(np.linspace(0, 1, 5), size=35)
    plt.legend(fontsize=28, loc="lower right")
    plt.grid(True)

    # Response Time Plot
    plt.subplot(132)  # 3 rows, 1 column, 2rd subplot
    plot_cdf(sched_1_response, label_1, "cornflowerblue", linestyle="solid")
    plot_cdf(sched_2_response, label_2, "lightcoral", linestyle="solid")
    plt.xlabel("Response Time (s)", size=35)
    # plt.ylabel("CDF", size=23)
    plt.xticks(np.arange(0, 8, step=2), size=35)
    plt.yticks(np.linspace(0, 1, 5), size=35)
    plt.legend(fontsize=28, loc="lower right")
    plt.grid(True)

    # Turnaround Time Plot
    plt.subplot(133)  # 3 rows, 1 column, 3rd subplot
    plot_cdf(sched_1_turnaround, label_1, "cornflowerblue", linestyle="solid")
    plot_cdf(sched_2_turnaround, label_2, "lightcoral", linestyle="solid")
    plt.xlabel("Turnaround Time (s)", size=35)
    # plt.ylabel("CDF", size=23)
    plt.xticks(np.arange(0, 20, step=4), size=35)
    plt.yticks(np.linspace(0, 1, 5), size=35)
    plt.legend(fontsize=28, loc="lower right")
    plt.grid(True)

    plt.tight_layout()
    plt.savefig(f"project/log/{sched_1}+{sched_2}_subplot.png")


def plot_execution_compare():
    _, _, CFS = time_measure("project/log/metrics_CFS.txt")
    _, _, FifoCfs_25 = time_measure("project/log/metrics_Hybrid_1633ms_25.txt")
    _, _, FifoCfs_26 = time_measure("project/log/metrics_Hybrid_1633ms_26.txt")
    _, _, FifoCfs_30 = time_measure("project/log/metrics_Hybrid_1633ms_30.txt")
    _, _, FifoCfs_40 = time_measure("project/log/metrics_Hybrid_1633ms_40.txt")

    # plot the CDF of execution time
    plt.figure(figsize=(9, 4.5))
    plot_cdf(CFS, "CFS(50)", "lightcoral", "dashed")
    plot_cdf(FifoCfs_25, "FIFO+CFS(25+25)", "salmon")
    plot_cdf(FifoCfs_26, "FIFO+CFS(26+24)", "limegreen")
    plot_cdf(FifoCfs_30, "FIFO+CFS(30+20)", "cornflowerblue")
    plot_cdf(FifoCfs_40, "FIFO+CFS(40+10)", "lightseagreen")

    plt.xlabel("Execution Time (seconds)", size=23)
    plt.ylabel("Cumulative prob", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    plt.legend(fontsize=20)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("project/log/cdf_execution_assemble.png")


def plot_execution_compare_ts():
    _, _, CFS = time_measure("project/log/metrics_CFS.txt")
    _, _, FifoCfs_p25 = time_measure(
        "project/results_backup/log_2mins/metrics_Hybrid_25_0.25.txt"
    )
    _, _, FifoCfs_p50 = time_measure(
        "project/results_backup/log_2mins/metrics_Hybrid_25_0.5.txt"
    )
    _, _, FifoCfs_p75 = time_measure(
        "project/results_backup/log_2mins/metrics_Hybrid_25_0.75.txt"
    )
    _, _, FifoCfs_p90 = time_measure(
        "project/results_backup/log_2mins/metrics_Hybrid_25_0.9.txt"
    )
    _, _, FifoCfs_p95 = time_measure(
        "project/results_backup/log_2mins/metrics_Hybrid_25_0.95.txt"
    )

    # plot the CDF of execution time
    plt.figure(figsize=(9, 4.5))
    plot_cdf(CFS, "CFS", "lightcoral", "dashed")
    plot_cdf(FifoCfs_p25, "FIFO(ts=p25)+CFS", "salmon")
    plot_cdf(FifoCfs_p50, "FIFO(ts=p50)+CFS", "limegreen")
    plot_cdf(FifoCfs_p75, "FIFO(ts=p75)+CFS", "cornflowerblue")
    plot_cdf(FifoCfs_p90, "FIFO(ts=p90)+CFS", "lightseagreen")
    plot_cdf(FifoCfs_p95, "FIFO(ts=p95)+CFS", "darkorange")

    plt.xlabel("Execution Time (seconds)", size=23)
    plt.ylabel("Cumulative prob", size=23)
    plt.xticks(size=20)
    plt.yticks(size=20)
    plt.legend(fontsize=20)
    plt.grid(True)
    plt.tight_layout()
    plt.savefig("project/log/cdf_execution_adaptbyduration.png")


def plot_preemption_count(sched_1, sched_2, label_1, label_2):
    cfs_preemptions = preemption_count(f"project/log/preempt_count_{sched_1}.txt")
    shortqueue_preemptions = preemption_count(
        f"project/log/preempt_count_{sched_2}.txt"
    )
    cpu_number = len(cfs_preemptions)

    # plot bar plot for preemption count
    plt.figure(figsize=(9, 4.5))
    cpu_labels = [f"{i}" for i in range(0, cpu_number)]

    cfs = [i - 0.2 for i in range(len(cpu_labels))]
    shortqueue = [i + 0.2 for i in range(len(cpu_labels))]

    plt.bar(cfs, cfs_preemptions, width=0.4, color="cornflowerblue", label=label_1)
    plt.bar(
        shortqueue,
        shortqueue_preemptions,
        width=0.4,
        color="lightcoral",
        label=label_2,
    )

    plt.yscale("log")
    plt.xlabel("Core ID", size=23)
    plt.ylabel("Preemption Count (Log)", size=23)
    visible_labels = range(0, len(cpu_labels), 5)
    plt.xticks(
        visible_labels,
        [cpu_labels[i] for i in visible_labels],
        size=20,
    )
    plt.yticks(size=20)
    plt.legend(fontsize=20)
    plt.tight_layout()
    plt.savefig(f"project/log/preemption_count_{sched_1}+{sched_2}.png")


if __name__ == "__main__":
    plot_utilization()
    plot_cdf_compare_subplot("FIFO", "FIFO_100ms", "FIFO", "FIFO_100")
    plot_cdf_compare_subplot("FIFO", "CFS", "FIFO", "CFS")
    plot_execution_compare()
    plot_execution_compare_ts()
    plot_utilization("FIFO", "FIFO", 2, 3)
    plot_utilization("FIFO", "CFS", 5, 6)
    plot_utilization("FIFO(ts=p95)", "CFS", 9, 10)
    plot_utilization("FIFO(ts=p75)", "CFS", 13, 14)
