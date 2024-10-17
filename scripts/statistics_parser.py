import re
import csv
import os
import sys

def process_block(block):
    stats = {}
    patterns = {
        "name": r"Processing:\s*(\w+)",
        "MHB_num": r"No of MHB constraints:\s*(\d+)",
        "MHB_gen_time": r"Generated MHB constraints, took:\s*(\d+)",
        "lock_num": r"No of lock constraints:\s*(\d+)",
        "lock_gen_time": r"Generated lock constraints, took:\s*(\d+)",
        "read_cf_num": r"No of read cf constraints:\s*(\d+)",
        "average_feasible_writes": r"Average no of feasible writes:\s*([\d.]+)",
        "average_infeasible_writes": r"Average no of infeasible writes:\s*([\d.]+)",
        "read_cf_gen_time": r"Generated read cf constraints, took:\s*(\d+)",
        "COP_num": r"No of COP events:\s*(\d+)",
        "MHB_add_time": r"added MHB constraints, took:\s*(\d+)",
        "lock_add_time": r"added lock constraints, took:\s*(\d+)",
        "read_cf_add_time": r"added read cf constraints, took:\s*(\d+)",
        "memory_used": r"Maximum resident set size \(kbytes\):\s*(\d+)",
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, block)
        stats[key] = match.group(1) if match else None

    return stats

def process_file(file_path, output_csv):
    with open(file_path, 'r') as file:
        data = file.read()

    blocks = data.split('----------------------------------------------------')

    with open(output_csv, 'w', newline='') as csvfile:
        fieldnames = [
            "name", "MHB_num", "MHB_gen_time", "lock_num", "lock_gen_time", 
            "read_cf_num", "average_feasible_writes", "average_infeasible_writes",
            "read_cf_gen_time", "MHB_add_time", "lock_add_time", "read_cf_add_time",
            "COP_num", "memory_used"
        ]
        writer = csv.DictWriter(csvfile, fieldnames=fieldnames)

        writer.writeheader()

        for block in blocks:
            if block.strip():
                stats = process_block(block)
                writer.writerow(stats)

    print(f"Processed data saved to {output_csv}")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python debug_parser.py <input_file>")
    else:
        input_file_path = sys.argv[1]
        
        if not os.path.exists(input_file_path):
            print(f"File not found: {input_file_path}")
            sys.exit(1)
        
        output_csv = os.path.splitext(input_file_path)[0] + '.csv'
        
        process_file(input_file_path, output_csv)
