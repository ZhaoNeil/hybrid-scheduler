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


def read_trace():
    # log the results to the usage_results.txt file
    command = f"python3 ../function_trace/read_trace.py --output throughput_results"
    print(command)
    subprocess.run(command, shell=True)


if __name__ == "__main__":
    sum_IAT("../log/throughput_results.txt")
    cpu_list = ["0-9", "0-19", "0-29", "0-39", "0-49"]
    for i in cpu_list:
        with open("../log/throughput_results.txt", "a") as f:
            f.write(f"CFS {i} ")
        launch_cfs(i)
        time.sleep(1)
