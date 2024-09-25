import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

durations_file = "../../Downloads/azurefunctions-dataset2019/function_durations_percentiles.anon.d01.csv"
invoke_file = "../../Downloads/azurefunctions-dataset2019/invocations_per_function_md.anon.d01.csv"

duration_df = pd.read_csv(durations_file).iloc[:, [2, 3]]

invoke_df = pd.read_csv(invoke_file).drop(columns=["HashOwner", "HashApp", "Trigger"])

df = pd.merge(duration_df, invoke_df, how="inner", on=["HashFunction"])

df = df[df["Average"] > 0].drop(columns=["HashFunction"])

count_per_min = df.drop(columns=["Average"]).sum(axis=0)

x = np.arange(1440)
y = np.array(count_per_min)
# bar plot of count per minute
fig, ax = plt.subplots(figsize=(6, 4.5))
plt.bar(x, y, color="cornflowerblue", width=1, alpha=1)

plt.xlabel("Minutes in one day", size=23)
plt.xlim(0, 1439)
plt.ylabel("Number of functions", size=23)

ax.set_yscale("log")
visible_labels = list(range(0, len(x), 240))
visible_positions = visible_labels + [1439]

plt.xticks(
    visible_positions,
    [str(x[i]) for i in visible_labels] + ["1440"],
    fontsize=20,
    rotation=45,
)
ax.tick_params(axis="y", which="minor", labelsize=20, labelrotation=45)
plt.tight_layout()

plt.savefig("./dayMemPattern/arrival_pattern_dur.png")
