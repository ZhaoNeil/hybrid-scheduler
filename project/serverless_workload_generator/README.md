# Serverless Workload Generator

This repository contains the code for generating serverless functions and arrival pattern analysis of paper [In Serverless, OS Scheduler Choice Costs Money: A Hybrid Scheduling Approach for Cheaper FaaS](), which is accepted by Middleware 2024.

---

### Files
- `arrival_pattern_dur.py`, plots the FaaS functions arrival pattern in one day.
- `duration_pattern.py`, plots the CDF of function duration in the first two minutes, shows the distribution of fibonacci number.
- `duration_plot.py`, plots the comparison of pattern in two minutes and two weeks.
- `func_duration.py`, generates the workload according to the Azure FaaS pattern.

---

### Dataset
The dataset is from [Microsoft Azure Functions Trace 2019](https://github.com/Azure/AzurePublicDataset/blob/master/AzureFunctionsDataset2019.md).
