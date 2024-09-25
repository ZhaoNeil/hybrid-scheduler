import time
import subprocess
import getpass
import sys

sys.path.append("..")
from utils.cpu_util import monitor_cpu_utilization

pwd = getpass.getpass("Enter sudo password: ")
# command to clean up the enclave
clean_up = "find /sys/fs/ghost/enclave_* -name ctl -exec sh -c 'echo destroy > {}' \\;"


# launch the hybrid scheduler
def launch_short_cfs(ghost_cpus, shortcpulist, preemption_time_slice):
    print("ghost cpus: ", ghost_cpus)
    print("short cpus: ", shortcpulist)
    print("preemption time slice: ", preemption_time_slice)
    working_dir = "/home/yuxuan/ghost-userspace/"
    command = [
        "sudo",
        "-S",
        "bash",
        "-c",
        f"./bazel-bin/hybrid_agent --ghost_cpus {ghost_cpus} --shortcpulist {shortcpulist} --preemption_time_slice {preemption_time_slice}",
    ]
    # Get the long list of CPUs
    longlist = f"{int(shortcpulist.split('-')[1])+1}-{ghost_cpus.split('-')[1]}"

    # Monitor the CPU utilization, output to the log/utilization.txt file
    @monitor_cpu_utilization(shortcpulist, longlist, "utilization")
    def decorated_read_trace():
        read_trace()

    # Cleanup the enclave
    cleanup_command = ["sudo", "-S", "bash", "-c", clean_up]
    process = subprocess.Popen(cleanup_command, stdin=subprocess.PIPE, cwd=working_dir)
    process.communicate(input=pwd.encode())  # Pass the password to sudo
    time.sleep(1)

    process = subprocess.Popen(command, stdin=subprocess.PIPE, cwd=working_dir)

    time.sleep(3)
    decorated_read_trace()

    try:
        process.wait()
    except KeyboardInterrupt:
        print("Ctrl+C interruption handled.")


# read the workload trace
def read_trace():
    # log the results to the log/tune_results.txt file
    command = f"python3 ../function_trace/read_trace.py --outputfile utilization"
    print(command)
    subprocess.run(command, shell=True)


if __name__ == "__main__":
    outputfile = "../log/utilization.txt"
    timeslice_list = ["1633ms"]
    ghost_cpus = "0-49"
    shortcpu_list = ["0-24"]
    for i in timeslice_list:
        for j in shortcpu_list:
            with open(outputfile, "a") as f:
                f.write(f"ghost_cpus: {ghost_cpus}, time slice: {i}, short list: {j}\n")
            launch_short_cfs(ghost_cpus, j, i)
    time.sleep(1)
