import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

durations_file = "../../Downloads/azurefunctions-dataset2019/function_durations_percentiles.anon.d01.csv"
invoke_file = "../../Downloads/azurefunctions-dataset2019/invocations_per_function_md.anon.d01.csv"

duration_df = pd.read_csv(durations_file).iloc[:, [2, 3]]

invoke_df = pd.read_csv(invoke_file).drop(columns=["HashOwner", "HashApp", "Trigger"])

df = pd.merge(duration_df, invoke_df, how="inner", on=["HashFunction"])

df = df[df["Average"] > 0]
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

# get the total number of function invocation for each duration
duration_occurance["Count"] = duration_occurance.sum(axis=1)
duration_occurance.reset_index(inplace=True)
print(duration_occurance)

# Get the 10%, 25%, 50%, 75%, 90%, 99% percentile of the function duration in first two minutes
min = 2
df_2min = duration_occurance.iloc[:, 0 : min + 1]
print(df_2min)
# get the sum of each row except the first column
df_2min["Count"] = df_2min.drop(columns=["Duration"]).sum(axis=1)
print(df_2min)
x = list(df_2min["Duration"])
y = list(df_2min["Count"])
p = y / np.sum(y)
cdf = np.cumsum(p)
fig, ax1 = plt.subplots(figsize=(9, 4.5))
x_divided = [value / 1000 for value in x]
ax1.step(x_divided, cdf, color="lightcoral", where="pre", linewidth=3)
plt.xticks(size=20)
plt.yticks(size=20)
plt.grid()
plt.xscale("log")
ax1.set_xlabel("Average Duration (s)", size=23)
ax1.set_ylabel("Cumulative prob", size=23)
plt.tight_layout()
plt.savefig("dayMemPattern/duration_cdf_2mins.png")

proportions = [0.1, 0.25, 0.5, 0.75, 0.9, 0.99]
for proportion in proportions:
    for i in cdf:
        if i >= proportion:
            index = cdf.tolist().index(i)
            print(
                f"{proportion * 100}% tasks is less than {x[index]} ms in first {min} minutes"
            )
            break


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

# x is the duration, y is the number of function invocation
# Get the percentage of the each fib N's
x_arr = np.array(x)
y_arr = np.array(y)
# bucket stores cumulative percentage of the function whose duration is less than the corresponding duration
bucket = []
for dur in dur_list:
    bucket.append(np.sum(y_arr[x_arr <= dur]) / np.sum(y_arr))

bucket = [0] + bucket + [1]
ratio = np.diff(np.array(bucket))

for i in range(len(dur_list)):
    if i == 0:
        print(f"0~{dur_list[i]}ms({fib[i]}): {ratio[i]*100}%")
    else:
        print(f"{dur_list[i-1]}ms~{dur_list[i]}ms ({fib[i]}): {ratio[i]*100}%")
print(f"{dur_list[-1]}ms~inf (46): {ratio[-1]*100}%")

# Get the percentage of the function whose duration is less than 1s
print(f"0~1000ms: {np.sum(y_arr[x_arr <= 1000]) / np.sum(y_arr)*100}%")
