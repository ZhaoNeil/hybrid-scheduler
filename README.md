# Hybrid Scheduler
This repository contains the artifacts of paper [In Serverless, OS Scheduler Choice Costs Money: A Hybrid Scheduling Approach for Cheaper FaaS](), which was accepted at Middleware 2024.

This repository is forked from: [ghost-userspace](https://github.com/google/ghost-userspace/commit/d4ddad4c307f2b34ee5e807e791072de29482bbe) and this README adapts from its.

---

### Dependencies

The ghOSt userspace component can be compiled on Ubuntu 20.04 or newer. Follow the instructions below:

1\. We use the Google Bazel build system to compile the userspace components of
ghOSt. Go to the
[Bazel Installation Guide](https://docs.bazel.build/versions/main/install.html)
for instructions to install Bazel on your operating system.

2\. Install ghOSt dependencies:

```
sudo apt update
sudo apt install libnuma-dev libcap-dev libelf-dev libbfd-dev gcc clang-12 llvm zlib1g-dev python-is-python3
```

Note that ghOSt requires GCC 9 or newer and Clang 12 or newer.

---

### Related Folders
Our implementation is based on the ghOSt system. This folder contains the content of our paper.
- `project/`
  - `exp/`, the experiment pipeline for testing multiple schedulers, throughput of the CFS, CPU utilization of hybird scheduler (with and without adapting CPU cores).
  - `function_trace/`, this folder contains the Fibonacci function, a CPU utilization monitor script, and a script for generating the Fibonacci function according to the Azure FaaS trace.
  - `log/`, this folder contains all the experiment logs and graphs.
  - `utils/`, this folder contains serveral analysis scripts, including plotting, counting preemption, calculating response time and cost, etc.
  - `serverless_workload_generator/`, the code for generating the workload according to the Azure FaaS pattern.
- `schedulers/`
  - ghOSt schedulers.
  - `hybrid/`, Our hybrid scheduler

---

### Running Hybrid Scheduler

We will run the per-CPU FIFO ghOSt scheduler and use it to schedule Linux
pthreads.

1. Build the hybrid scheduler:
```
bazel build -c opt hybrid_agent
```

2. Launch the hybrid scheduler:
```
sudo bazel-bin/hybrid_agent --ghost_cpus 0-49 --shortcpulist 0-24 --preemption_time_slice 1633ms
```
The scheduler launches ghOSt agents on CPUs (i.e., logical cores) 0 to 49 and
will therefore schedule ghOSt tasks onto CPUs 0 and 49. Adjust the `--ghost_cpus`
command line argument value as necessary. For example, if you have an 8-core
machine and you wish to schedule ghOSt tasks on all cores, then pass `0-7` to
`--ghost_cpus`. `--shortcpulist` command line argument represents the cores assigned to the FIFO policy. `--preemption_time_slice` command line argument represents the time limit for the tasks on the FIFO cores.

3. Launch Azure FaaS workloads:
Open a new terminal and run the command to launch the Azure FaaS workloads.
```
python3 project/function_trace/read_trace.py
```

4. Use `Ctrl-C` to send a `SIGINT` signal to `fifo_per_cpu_agent` to get it to stop.

5. To kill all enclaves (which is generally useful in development), run the
following command:
```
for i in /sys/fs/ghost/enclave_*/ctl; do echo destroy > $i; done
```

---
### Our Workloads
Our workloads is generated from the [Microsoft Azure Functions Trace 2019](https://github.com/Azure/AzurePublicDataset/blob/master/AzureFunctionsDataset2019.md). More details could be found in `serverless_workload_generator` folder.
