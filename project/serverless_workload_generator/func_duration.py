import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.axes_grid1.inset_locator import inset_axes, mark_inset

durations_file = "../../Downloads/azurefunctions-dataset2019/function_durations_percentiles.anon.d01.csv"
invoke_file = "../../Downloads/azurefunctions-dataset2019/invocations_per_function_md.anon.d01.csv"
workload_file = "workload_dur.txt"

duration_df = pd.read_csv(durations_file).iloc[:, [2, 3]]

invoke_df = pd.read_csv(invoke_file).drop(columns=["HashOwner", "HashApp", "Trigger"])

# Merge the duration and invocation dataframes by the HashFunction column
df = pd.merge(duration_df, invoke_df, how="inner", on=["HashFunction"])

df = df[df["Average"] > 0]
df = df[df["Average"] < 1000000]
df = df.sort_values(by="Average")
# print(df)

# get the number of function invocation in each minute, key is the function duration
duration_dict = {}
for index, row in df.iterrows():
    duration = list(row)[1]
    occur_list = list(row)[2:]
    if duration not in duration_dict:
        duration_dict[duration] = occur_list
    else:
        duration_dict[duration] = list(
            map(lambda x: x[0] + x[1], zip(duration_dict[duration], occur_list))
        )

# convert the dictionary to dataframe
duration_occurance = pd.DataFrame.from_dict(duration_dict, orient="index")
duration_occurance.index.name = "Duration"
duration_occurance.reset_index(inplace=True)
print(duration_occurance)

bucket = {}
for i in range(29, 47):
    bucket[i] = [0] * 1440

# According to calibration, function duration and the corresponding fib N's
dur_list = [
    8,
    11,
    13,
    15,
    17,
    23,
    34,
    56,
    81,
    125,
    209,
    215,
    354,
    533,
    934,
    1393,
    2457,
    3653,
]
fib = [29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46]

for index, row in duration_occurance.iterrows():
    Duration = list(row)[0]
    occur_list = list(row)[1:]
    for i in range(len(dur_list)):
        if Duration <= dur_list[i] or i == len(dur_list) - 1 and Duration > dur_list[i]:
            # Bucket the function invocation based on the duration
            bucket[fib[i]] = list(
                map(lambda x: x[0] + x[1], zip(bucket[fib[i]], occur_list))
            )
            break
    # if Duration <= 27:
    #     bucket[35] = list(map(lambda x: x[0] + x[1], zip(bucket[35], occur_list)))
    # elif Duration > 27 and Duration <= 37:
    #     bucket[36] = list(map(lambda x: x[0] + x[1], zip(bucket[36], occur_list)))
    # elif Duration > 37 and Duration <= 64:
    #     bucket[37] = list(map(lambda x: x[0] + x[1], zip(bucket[37], occur_list)))
    # ......


arg_df = pd.DataFrame.from_dict(bucket, orient="index")
arg_df.index.name = "arg"
print(arg_df)

occur_time = []

# Generate the workload item for each minute
for minute in arg_df.columns[0:2]:
    for arg in arg_df.index:
        invoke_times = arg_df.loc[arg, minute] / 100  # downscale
        interval = 60 / invoke_times
        for n in range(int(invoke_times)):
            time = int(minute) * 60 + n * interval
            occur_time.append((time, str(arg)))

# Sort the items by time
sort_list = sorted(occur_time, key=lambda x: x[0])
time_list, arg_list = zip(*sort_list)
# Get the time difference between each item, which is the inter-arrival time
diff_list = time_list[0] + np.diff(list(time_list))
output_list = list(zip(diff_list, arg_list))

# Write the workload to a file
f = open(workload_file, "w")
for t in output_list:
    line = " ".join(str(x) for x in t)
    f.write(line + "\n")
f.close()
