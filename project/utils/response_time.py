from datetime import datetime


# Remove timezone offset information
def parse_datetime(timestamp):
    without_timezone = timestamp.split("+")[0]
    # truncate milliseconds to 6 digits
    dot_index = without_timezone.find(".")

    if dot_index != -1:  # If there's a fractional part
        # Separate the timestamp into the main part and the fractional seconds
        main_part = without_timezone[:dot_index]
        fractional_seconds = without_timezone[dot_index + 1 :]

        # Truncate to 6 digits or pad with zeros to reach 6 digits
        fractional_seconds = (fractional_seconds + "000000")[:6]

        # Reassemble the timestamp with the adjusted fractional part
        without_timezone = f"{main_part}.{fractional_seconds}"
    else:
        # If there's no fractional part, add one with six zeros
        without_timezone += ".000000"
    return datetime.strptime(without_timezone, "%Y-%m-%dT%H:%M:%S.%f")


def time_measure(log_file):
    task_info = {}
    with open(log_file, "r") as file:
        for line in file:
            task_id = line.split()[1]
            info_type = line.split()[0]
            if info_type == "TaskNew:":
                time = parse_datetime(line.split()[7])
                task_info[task_id] = {"create_time": time}
            elif info_type == "FirstRun:":
                time = parse_datetime(line.split()[7])
                task_info[task_id]["first_run"] = time
            elif info_type == "TaskDead:":
                time = parse_datetime(line.split()[3])
                task_info[task_id]["complete_time"] = time

    response_time = []
    turnaround_time = []
    execution_time = []
    for task_id, info in task_info.items():
        response_time.append(
            round((info["first_run"] - info["create_time"]).total_seconds(), 2)
        )
        turnaround_time.append(
            round((info["complete_time"] - info["create_time"]).total_seconds(), 2)
        )
        execution_time.append(
            round((info["complete_time"] - info["first_run"]).total_seconds(), 2)
        )

    return response_time, turnaround_time, execution_time
