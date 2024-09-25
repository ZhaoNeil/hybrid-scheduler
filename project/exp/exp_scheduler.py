import time
import subprocess
import getpass
import sys

sys.path.append("..")
from utils.sum_IAT import sum_IAT

pwd = getpass.getpass("Enter sudo password: ")
# command to clean up the enclave
clean_up = "find /sys/fs/ghost/enclave_* -name ctl -exec sh -c 'echo destroy > {}' \\;"


# launch the cfs scheduler
def launch_cfs(ghost_cpus):
    print("ghost cpus: ", ghost_cpus)
    min_granularity = "1ms"
    print("min granularity: ", min_granularity)
    latency = "10ms"
    print("latency: ", latency)
    command = f"../../bazel-bin/cfs_agent --ghost_cpus {ghost_cpus} --min_granularity {min_granularity} --latency {latency} &"

    # clean up the enclave and launch the cfs scheduler
    subprocess.run("echo {} | sudo -S {}".format(pwd, clean_up), shell=True)
    time.sleep(1)
    print(command)
    subprocess.run("echo {} | sudo -S {}".format(pwd, command), shell=True)

    time.sleep(10)
    read_trace()


# launch the Earliest Deadline First scheduler
def launch_edf(ghost_cpus):
    print("ghost cpus: ", ghost_cpus)
    command = f"../../bazel-bin/agent_exp --ghost_cpus {ghost_cpus} &"

    # clean up the enclave and launch the edf scheduler
    subprocess.run("echo {} | sudo -S {}".format(pwd, clean_up), shell=True)
    time.sleep(1)
    print(command)
    subprocess.run("echo {} | sudo -S {}".format(pwd, command), shell=True)

    time.sleep(10)
    read_trace()


# launch the fifo centralized scheduler
def launch_fifo_centralized(ghost_cpus):
    print("ghost cpus: ", ghost_cpus)
    preemption_time_slice = ""  # default is infinite time slice
    print("preemption time slice: ", preemption_time_slice)

    if preemption_time_slice:
        command = f"../../bazel-bin/fifo_centralized_agent --ghost_cpus {ghost_cpus} --preemption_time_slice {preemption_time_slice} &"
    else:
        command = f"../../bazel-bin/fifo_centralized_agent --ghost_cpus {ghost_cpus} &"

    # clean up the enclave and launch the fifo centralized scheduler
    subprocess.run("echo {} | sudo -S {}".format(pwd, clean_up), shell=True)
    time.sleep(1)
    print(command)
    subprocess.run("echo {} | sudo -S {}".format(pwd, command), shell=True)

    time.sleep(10)
    read_trace()


# launch the fifo per cpu scheduler
def launch_fifo_per_cpu(ghost_cpus):
    print("ghost cpus: ", ghost_cpus)
    command = f"../../bazel-bin/fifo_per_cpu_agent --ghost_cpus {ghost_cpus} &"

    # clean up the enclave and launch the fifo per cpu scheduler
    subprocess.run("echo {} | sudo -S {}".format(pwd, clean_up), shell=True)
    time.sleep(1)
    print(command)
    subprocess.run("echo {} | sudo -S {}".format(pwd, command), shell=True)

    time.sleep(10)
    read_trace()


# launch the Speed-of-Light scheduler
def launch_sol(ghost_cpus):
    print("ghost cpus: ", ghost_cpus)
    preemption_time_slice = ""  # default is infinite time slice
    print("preemption time slice: ", preemption_time_slice)

    if preemption_time_slice:
        command = f"../../bazel-bin/agent_sol --ghost_cpus {ghost_cpus} --preemption_time_slice {preemption_time_slice} &"
    else:
        command = f"../../bazel-bin/agent_sol --ghost_cpus {ghost_cpus}&"

    # clean up the enclave and launch the sol scheduler
    subprocess.run("echo {} | sudo -S {}".format(pwd, clean_up), shell=True)
    time.sleep(1)
    print(command)
    subprocess.run("echo {} | sudo -S {}".format(pwd, command), shell=True)

    time.sleep(10)
    read_trace()


# read the workload trace
def read_trace():
    # log the results to the scheduler_results.txt file
    command = f"python3 ../function_trace/read_trace.py --outputfile scheduler_results"
    print(command)
    subprocess.run(command, shell=True)


if __name__ == "__main__":
    sum_IAT("../log/scheduler_results.txt")
    with open("../log/scheduler_results.txt", "a") as f:
        f.write("cfs 0-39 ")
    launch_cfs("0-39")
    time.sleep(1)
    with open("../log/scheduler_results.txt", "a") as f:
        f.write("edf 0-39 ")
    launch_edf("0-39")
    time.sleep(1)
    with open("../log/scheduler_results.txt", "a") as f:
        f.write("fifo centralized 0-39 ")
    launch_fifo_centralized("0-39")
    time.sleep(1)
    with open("../log/scheduler_results.txt", "a") as f:
        f.write("fifo per cpu 0-39 ")
    launch_fifo_per_cpu("0-39")
    time.sleep(1)
    with open("../log/scheduler_results.txt", "a") as f:
        f.write("sol 0-39 ")
    launch_sol("0-39")
