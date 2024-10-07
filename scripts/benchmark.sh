#!/bin/bash
INPUT_DIR="$PWD/traces/formatted_traces"
PROGRAM="$PWD/bin/rvpredict"
OUTPUT_FILE="$PWD/output/rvpredict_results.txt"
LINE_COUNT_FILE="$PWD/output/line_count.txt"

TIME_LIMIT=300

> "$OUTPUT_FILE"

# declare -A line_counts
# while IFS=" - " read -r filename line_count; do
#     line_counts["$filename"]=$line_count
# done < "$LINE_COUNT_FILE"

for FILE in "$INPUT_DIR"/*; do
    BASENAME=$(basename "$FILE")
    
    echo "Processing: $BASENAME" >> "$OUTPUT_FILE"
    
    # if [ "${line_counts[$BASENAME]}" ]; then
    #     echo "Line count for $BASENAME: ${line_counts[$BASENAME]}" >> "$OUTPUT_FILE"
    # else
    #     echo "Line count for $BASENAME: Not found" >> "$OUTPUT_FILE"
    # fi
    
    timeout $TIME_LIMIT "$PROGRAM" "$FILE" >> "$OUTPUT_FILE" 2>&1
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 124 ]; then
        echo "The program timed out for file: $FILE" >> "$OUTPUT_FILE"
    elif [ $EXIT_CODE -ne 0 ]; then
        echo "The program encountered an error (exit code: $EXIT_CODE) for file: $FILE" >> "$OUTPUT_FILE"
    fi
    
    echo "----------------------------------------------------" >> "$OUTPUT_FILE"
done

echo "Execution completed. Output is stored in $OUTPUT_FILE."
