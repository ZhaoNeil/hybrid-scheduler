import time
import psutil
import threading
import concurrent.futures


# Get the average CPU utilization for a given range of CPUs
def get_average(cpu_start, cpu_end):
    cpu_utilization = psutil.cpu_percent(interval=1, percpu=True)
    avg_utilization = sum(cpu_utilization[cpu_start : cpu_end + 1]) / (
        cpu_end - cpu_start + 1
    )
    return avg_utilization


# Monitor the CPU utilization for a given range of CPUs
def monitor_cpu_utilization(shortlist, longlist, outputfile):
    def decorator(func):
        def wrapper(*args, **kwargs):
            event.clear()
            thread = threading.Thread(
                target=get_cpu_utilization,
                args=(shortlist, longlist, outputfile),
            )
            thread.start()
            result = func(*args, **kwargs)
            event.set()
            thread.join()
            return result

        def get_cpu_utilization(shortlist, longlist, outputfile):
            # Get the range of CPUs
            short_start = int(shortlist.split("-")[0])
            short_end = int(shortlist.split("-")[1])
            long_start = int(longlist.split("-")[0])
            long_end = int(longlist.split("-")[1])

            # Collect the CPU utilization when the function is running
            short_avg = []
            long_avg = []
            while not event.is_set():
                # Use ThreadPoolExecutor to get the average CPU utilization for the short
                # and long list of CPUs, or the number of the sampling points is half of
                # the actual number
                with concurrent.futures.ThreadPoolExecutor() as executor:
                    thread_short = executor.submit(get_average, short_start, short_end)
                    thread_long = executor.submit(get_average, long_start, long_end)
                    short_avg.append(thread_short.result())
                    long_avg.append(thread_long.result())

            # Log the results to the output file
            with open(f"../log/{outputfile}.txt", "a") as f:
                f.write(f"short average CPU utilization: {short_avg}\n")
                f.write(f"long average CPU utilization: {long_avg}\n")

        event = threading.Event()
        return wrapper

    return decorator


# Get the per CPU utilization for a given range of CPUs
def get_per_cpu_utilization(cpu_start, cpu_end, event):
    while not event.is_set():
        cpu_utilization = psutil.cpu_percent(interval=1, percpu=True)[
            cpu_start : cpu_end + 1
        ]
        with open("../log/utilization_adapt.txt", "a") as f:
            f.write(f"{cpu_utilization}\n")


# Monitor the per CPU utilization for a given range of CPUs
def monitor_per_cpu_utilization(cpu_start, cpu_end):
    def decorator(func):
        def wrapper(*args, **kwargs):
            event = threading.Event()
            thread = threading.Thread(
                target=get_per_cpu_utilization,
                args=(cpu_start, cpu_end, event),
            )
            thread.start()
            try:
                result = func(*args, **kwargs)
            finally:
                event.set()
                thread.join()
            return result

        return wrapper

    return decorator
