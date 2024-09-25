import time
import subprocess


# The function to launch the C++ fibonacci function
def launch_command_cpp(arg):
    command = (
        f"/home/yuxuan/ghost-userspace/project/function_trace/launch_function.out {arg}"
    )
    print(command)
    subprocess.run(command, shell=True)


dur_list = []
fib = []


# Measure the runtime of the C++ fibonacci function
def loop(arg, repeat):
    start = time.time()
    for i in range(repeat):
        launch_command_cpp(arg)
    end = time.time()
    # get milliseconds
    print("Runtime for arg {} is {} ms".format(arg, (end - start) * 1000 / repeat))
    dur_list.append(round((end - start) * 1000 / repeat))
    fib.append(arg)
    with open("../log/calibrate.txt", "a") as f:
        f.write(
            "Runtime for arg {} is {} ms\n".format(arg, (end - start) * 1000 / repeat)
        )


if __name__ == "__main__":
    repeat = 100
    for i in range(29, 30):
        loop(i, repeat)
    with open("../log/calibrate.txt", "a") as f:
        f.write("dur_list = {}\n".format(dur_list))
        f.write("fib = {}\n".format(fib))
