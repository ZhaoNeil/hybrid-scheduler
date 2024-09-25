import re
import time
import argparse
import asyncio


# Launch the C++ fibonacci function
async def launch_command_cpp(arg):
    command = (
        f"/home/yuxuan/ghost-userspace/project/function_trace/launch_function.out {arg}"
    )
    # print(command)
    process = await asyncio.create_subprocess_shell(command)
    await process.communicate()


async def launch_command_firecraker(firecracker_id, time):
    command = f"/home/shared/serverlessinterface/cold {firecracker_id} {time}"
    print(command)
    process = await asyncio.create_subprocess_shell(command)
    await process.communicate()


# Launch the C++ fibonacci function according to the trace file IAT
async def main(outputfile):
    tasks = []
    # Read trace file
    with open(
        "/home/yuxuan/ghost-userspace/project/function_trace/trace.txt", "r"
    ) as f:
        start = time.time()
        lines = f.readlines()
        for line in lines:
            IAT = float(line.split(" ")[0])
            arg = int(line.split(" ")[1])  # arg is fibonacci N
            await asyncio.sleep(IAT)  # sleep for IAT seconds
            task = asyncio.create_task(launch_command_cpp(arg))
            tasks.append(task)

    # Wait for all tasks to complete
    await asyncio.gather(*tasks)

    end = time.time()
    print("Time elapsed: {:.2f} s".format(end - start))
    # log the results to the output file
    with open(f"../log/{outputfile}.txt", "a") as f:
        f.write(f"time elapsed: {end - start} s\n")


# Launch the C++ fibonacci function according to the trace file IAT
async def main_firecracker(outputfile):
    tasks = []
    # Read trace file
    with open(
        "/home/yuxuan/ghost-userspace/project/function_trace/trace.txt", "r"
    ) as f:
        start = time.time()
        lines = f.readlines()
        for i, line in enumerate(lines):
            IAT = float(line.split(" ")[0])
            arg = int(line.split(" ")[1])  # arg is fibonacci N
            await asyncio.sleep(IAT)  # sleep for IAT seconds
            delay = map(arg)
            task = asyncio.create_task(launch_command_firecraker(i, delay))
            tasks.append(task)

    # Wait for all tasks to complete
    await asyncio.gather(*tasks)

    end = time.time()
    print("Time elapsed: {:.2f} s".format(end - start))
    # log the results to the output file
    with open(f"../log/{outputfile}.txt", "a") as f:
        f.write(f"time elapsed: {end - start} s\n")


def map(arg):
    if arg <= 44:
        return 1
    elif arg == 45:
        return 2
    elif arg == 46:
        return 3


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--outputfile", type=str)
    args = parser.parse_args()
    outputfile = args.outputfile
    asyncio.run(main(outputfile))
    # asyncio.run(main_firecracker(outputfile))
