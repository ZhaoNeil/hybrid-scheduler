import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

dur_df_list = []
days = [
    "01",
    "02",
    "03",
    "04",
    "05",
    "06",
    "07",
    "08",
    "09",
    "10",
    "11",
    "12",
    "13",
    "14",
]
for i in days:
    dur_filename = f"../../Downloads/azurefunctions-dataset2019/function_durations_percentiles.anon.d{i}.csv"
    print(dur_filename)

    dur_df = pd.read_csv(dur_filename)
    dur_df_list.append(dur_df)

duration_df = pd.concat(dur_df_list, axis=0, ignore_index=True).iloc[:, [3, 4]]

duration_df = duration_df[duration_df["Average"] > 0]

duration_df = duration_df.groupby("Average").sum().reset_index()
print(duration_df)

# plot the CDF of function duration in two weeks
x_2weeks = list(duration_df["Average"] / 1000)
y_2weeks = list(duration_df["Count"])
p_2weeks = y_2weeks / np.sum(y_2weeks)
cdf_2weeks = np.cumsum(p_2weeks)
fig, ax1 = plt.subplots(figsize=(4.5, 4.5))
ax1.step(x_2weeks, cdf_2weeks, color="lightcoral", where="pre", linewidth=3)
plt.xticks(size=20, rotation=45)
plt.yticks(size=20)
plt.grid()
plt.xscale("log")
ax1.set_xlabel("Average Duration (s)", size=23)
ax1.set_ylabel("Cumulative prob", size=23)
plt.tight_layout()
plt.savefig("dayMemPattern/duration_cdf_2weeks.png")

# Get the percentage of the function whose duration is less than 1s
x_2weeks_arr = np.array(x_2weeks)
y_2weeks_arr = np.array(y_2weeks)
print(
    f"0~1000ms: {np.sum(y_2weeks_arr[x_2weeks_arr <= 1]) / np.sum(y_2weeks_arr)*100}%"
)


# Get the CFD in first two minutes
durations_day01 = "../../Downloads/azurefunctions-dataset2019/function_durations_percentiles.anon.d01.csv"
invoke_day01 = "../../Downloads/azurefunctions-dataset2019/invocations_per_function_md.anon.d01.csv"

duration_01 = pd.read_csv(durations_day01).iloc[:, [2, 3]]

invoke_01 = pd.read_csv(invoke_day01).drop(columns=["HashOwner", "HashApp", "Trigger"])

df = pd.merge(duration_01, invoke_01, how="inner", on=["HashFunction"])

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

# Get the function duration in first two minutes
min = 2
df_2min = duration_occurance.iloc[:, 0 : min + 1]
# get the sum of each row except the first column
df_2min["Count"] = df_2min.drop(columns=["Duration"]).sum(axis=1)
x = list(df_2min["Duration"])
y = list(df_2min["Count"])
p = y / np.sum(y)
cdf = np.cumsum(p)
fig, ax1 = plt.subplots(figsize=(9, 4.5))
x_divided = [value / 1000 for value in x]
ax1.step(
    x_2weeks,
    cdf_2weeks,
    color="lightcoral",
    where="pre",
    linewidth=3,
    label="Azure two weeks data",
)
ax1.step(
    x_divided,
    cdf,
    color="cornflowerblue",
    where="pre",
    linewidth=3,
    label="Our sampled data",
)
plt.xticks(size=20)
plt.yticks(size=20)
plt.grid()
plt.xscale("log")
ax1.set_xlabel("Average Duration (s)", size=23)
ax1.set_ylabel("Cumulative prob", size=23)
plt.legend(fontsize=20)
plt.tight_layout()
plt.savefig("dayMemPattern/duration_cdf.png")
