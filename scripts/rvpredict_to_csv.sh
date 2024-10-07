#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <file_type> <input_file>"
    exit 1
fi

file_type="$1"
input_file="$2"

output_file="${input_file%.*}.csv"

echo "test_case,no_of_races,time_taken" > "$output_file"

process_type1() {
    while IFS= read -r line; do
        if [[ $line == "Processing:"* ]]; then
            file_name=$(echo "$line" | cut -d ' ' -f 2)
            file_name="${file_name%.txt}"
        elif [[ $line == "Number of races detected:"* ]]; then
            num_races=$(echo "$line" | cut -d ' ' -f 5)
        elif [[ $line == "Time taken:"* ]]; then
            time_taken=$(echo "$line" | sed 's/Time taken: //g' | sed 's/ms//g')
            echo "$file_name,$num_races,$time_taken" >> "$output_file"
        elif [[ $line == *"timed out"* ]]; then
            echo "$file_name,-1,-1" >> "$output_file"
        elif [[ $line == *"encountered an error"* ]]; then
            echo "$file_name,-1,-1" >> "$output_file"
        fi
    done < "$input_file"
}

process_type2() {
    while IFS= read -r line; do
        if [[ $line == "Processing "* ]]; then
            file_name=$(basename "$(echo "$line" | cut -d ' ' -f 2)")
            file_name="${file_name%.std}"
        elif [[ $line == "Number of 'racy' events found = "* ]]; then
            num_races=$(echo "$line" | cut -d ' ' -f 7)
        elif [[ $line == "Time for analysis = "* ]]; then
            time_taken=$(echo "$line" | cut -d ' ' -f 5)
            time_taken=${time_taken% milliseconds}
            echo "$file_name,$num_races,$time_taken" >> "$output_file"
        fi
    done < "$input_file"
}

if [[ $file_type == "custom" ]]; then
    process_type1
elif [[ $file_type == "rapid" ]]; then
    process_type2
else
    echo "Invalid file type. Please use 'custom' or 'rapid'."
    exit 1
fi

echo "Conversion completed. CSV saved to $output_file."
