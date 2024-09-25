def preemption_count(file):
    preemption_counts = {}
    with open(file, "r") as file:
        for line in file:
            parts = line.split()
            cpu = parts[1]
            preemptions = int(parts[3])

            if cpu not in preemption_counts:
                preemption_counts[cpu] = preemptions

    sorted_cpus = sorted(preemption_counts.items(), key=lambda x: int(x[0]))
    result_list = [count for _, count in sorted_cpus]
    return result_list


def read_values_after_last_preemption(filename):
    values = []
    path = "project/log/" + filename
    with open(path, "r") as file:
        lines = file.readlines()
        last_preemption_index = max(
            i for i, line in enumerate(lines) if "preemption" in line
        )
        for line in lines[last_preemption_index + 1 :]:
            values.append(int(line.strip()) / 1000)
    return values
