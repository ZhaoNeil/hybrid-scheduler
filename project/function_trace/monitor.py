import psutil
import mmap
import time
import struct
import subprocess
import threading
import posix_ipc


# Get the CPU utilization for a given range of CPUs
def get_util(cpu_start, cpu_end):
    cpu_utilization = psutil.cpu_percent(interval=1, percpu=True)[
        cpu_start : cpu_end + 1
    ]
    # print(cpu_utilization)
    return cpu_utilization


def write_shm():
    # cpu_util = [0.0] * 50

    semaphore = posix_ipc.Semaphore(
        "/cpu_usage_sem", posix_ipc.O_CREAT, initial_value=1
    )

    shm = posix_ipc.SharedMemory("/cpu_usage", posix_ipc.O_CREAT, size=200)

    # create a memory map of the file
    map_file = mmap.mmap(shm.fd, shm.size)
    shm.close_fd()

    try:
        while True:
            semaphore.acquire()
            try:
                map_file.seek(0)
                cpu_util = get_util(0, 49)
                print(cpu_util)
                map_file.write(struct.pack(f"{len(cpu_util)}f", *cpu_util))
            finally:
                semaphore.release()
            time.sleep(1)
    finally:
        map_file.close()
        semaphore.unlink()
        shm.unlink()


def start_monitor(func):
    def wrapper(*args, **kwargs):
        stop_event = threading.Event()
        thread = threading.Thread(target=write_shm, args=(stop_event,))
        thread.start()

        result = func(*args, **kwargs)

        stop_event.set()
        thread.join()

        return result

    return wrapper


@start_monitor
def read_trace():
    command = f"python3 ../function_trace/read_trace.py"
    print(command)
    subprocess.run(command, shell=True)


if __name__ == "__main__":
    # read_trace()
    write_shm()
