import random
import os
import sys

"""
Example input:
T2|fork(T1)|0
T1|join(T21)|358
T20|r(V16e92358.199)|374
T20|w(V16e92358.199)|374 or T20|w(V45c470d5[0])|362
T20|acq(L2a45c47085)|361
T20|rel(L2a45c47085)|361
"""


def extract_thread_id(thread, debug=False):
    return int(thread[1:])


def map_action(action_part, debug=False):
    value = action_part.split("(")[1][:-1]
    action = ""
    if action_part.startswith("acq"):
        action = "Acq"
    elif action_part.startswith("rel"):
        action = "Rel"
    elif action_part.startswith(
        "r"
    ):  # r must be after rel to ensure release ops dont get mapped to read
        action = "Read"
    elif action_part.startswith("w"):
        action = "Write"
    elif action_part.startswith("fork"):
        action = "Fork"
    elif action_part.startswith("join"):
        action = "Join"
    else:
        raise ValueError(f"Unknown action: {action_part}")

    return action, value


locks = {}
lock_identifier = 0


def get_lock_id(lock, debug=False):
    if lock not in locks:
        global lock_identifier
        locks[lock] = lock_identifier
        lock_identifier += 1
    return locks[lock]


vars = {}
var_identifier = 0


def get_var_id(var, debug=False):
    if var not in vars:
        global var_identifier
        vars[var] = var_identifier
        var_identifier += 1
    return vars[var]


var_values = {}


def get_var_value(var, action, debug=False):
    if var not in var_values:
        var_values[var] = random.randint(0, 100)
    if action == "Write":
        var_values[var] = random.randint(0, 100)

    return var_values[var]


def action_string(action, value, debug=False):
    res = ""

    if action == "Read":
        res = f"X_{get_var_id(value, debug)} {get_var_value(value, action, debug)}"
    elif action == "Write":
        res = f"X_{get_var_id(value, debug)} {get_var_value(value, action, debug)}"
    elif action == "Fork":
        res = f"{value[1:]} 0"
    elif action == "Join":
        res = f"{value[1:]} 0"
    elif action == "Acq":
        res = f"l_{get_lock_id(value)} 0"
    elif action == "Rel":
        res = f"l_{get_lock_id(value)} 0"

    return res


def parse_trace_line(line, debug=False):
    parts = line.split("|")
    thread = parts[0]
    action_part = parts[1]

    thread_id = extract_thread_id(thread, debug)
    action, value = map_action(action_part, debug)
    action_str = action_string(action, value, debug)

    res = f"{action} {thread_id} {action_str}"

    if action == "Fork":
        res = res + "\n" + f"Begin {value[1:]} 0 0"
    elif action == "Join":
        res = f"End {value[1:]} 0 0" + "\n" + res

    return res


def convert_trace(trace_lines, debug=False):
    output_lines = []
    for line in trace_lines:
        try:
            new_line = parse_trace_line(line.strip(), debug)
            output_lines.append(new_line)
        except Exception as e:
            print(f"Error parsing line: {line}", end="")
            raise e

    return "\n".join(output_lines)


def convert_trace_file(input_file, output_file, debug=False):
    with open(input_file, "r") as f:
        trace_lines = f.readlines()

    try:
        converted_trace = convert_trace(trace_lines, debug)
    except Exception as e:
        return False

    with open(output_file, "w") as f:
        f.write(converted_trace)

    return True


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python convert.py <input_file> <output_dir>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_dir = sys.argv[2]
    filename = os.path.basename(input_file)

    if not filename.endswith(".std"):
        print("Input file must have .std extension")
        sys.exit(1)

    output_file = os.path.join(output_dir, filename.split(".")[0] + ".txt")

    if convert_trace_file(input_file, output_file, True):
        print(f"Converted {filename}")
    else:
        print(f"Error converting {filename}")
