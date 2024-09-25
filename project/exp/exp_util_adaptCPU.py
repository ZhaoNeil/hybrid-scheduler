import time
import subprocess
import getpass
import sys

sys.path.append("..")
from utils.cpu_util import monitor_per_cpu_utilization

pwd = getpass.getpass("Enter sudo password: ")
# command to clean up the enclave
clean_up = "find /sys/fs/ghost/enclave_* -name ctl -exec sh -c 'echo destroy > {}' \\;"


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

    # Monitor the CPU utilization, output to the file
    @monitor_per_cpu_utilization(0, 49)
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
    # log the results to the file
    command = f"python3 ../function_trace/read_trace.py"
    print(command)
    subprocess.run(command, shell=True)


if __name__ == "__main__":
    launch_short_cfs("0-49", "0-24", "1633ms")
