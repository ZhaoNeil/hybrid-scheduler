import os
import sys
import time
import subprocess

def get_pid():
    pid = os.getpid()
    return pid

# fibbonacci function
def fib(n):
    if n < 2:
        return n
    else:
        return fib(n-1) + fib(n-2)


def launch_function(num):
    current_pid = get_pid()
    command = f"echo {current_pid} > /sys/fs/ghost/enclave_1/tasks"
    print(command)
    subprocess.call(command, shell=True)

    fib(num)

    print("Finished!")


if __name__ == "__main__":
    num = int(sys.argv[1])
    launch_function(num)