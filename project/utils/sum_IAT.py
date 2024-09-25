# Get the sum of IAT from trace.txt
def sum_IAT(output):
    sum = 0

    with open(
        "/home/yuxuan/ghost-userspace/project/function_trace/trace.txt", "r"
    ) as f:
        lines = f.readlines()
        for line in lines:
            IAT = float(line.split(" ")[0])
            sum += IAT

    print("IAT sum is: ", sum, "s")

    with open(output, "a") as f:
        f.write("IAT sum is: {} s\n".format(sum))
