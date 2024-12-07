#!/bin/bash
INPUT_DIR="$PWD/traces/formatted_traces"
PROGRAM="$PWD/bin/predictor"

TIME_LIMIT=600

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <output_file>"
    exit 1
fi

OUTPUT_FILE="$PWD/output/$1.txt"
> "$OUTPUT_FILE"

LONG_TRACES=(
    "cryptorsa" 
    "linkedlist" 
    "lusearch" 
    "xalan" 
    "bufwriter" 
    "moldyn" 
    "readerswriters" 
    "ftpserver" 
    "derby" 
    "jigsaw"
)

# Under 2000 events
SHORT_TRACES=(
    "account" 
    "airlinetickets" 
    "array" 
    "boundedbuffer" 
    "bubblesort" 
    "clean" 
    "critical" 
    "lang" 
    "mergesort" 
    "pingpong" 
    "producerconsumer" 
    "raytracer" 
    "twostage" 
    "wronglock"
)

is_long_trace() {
    local file_basename=$1
    for long_trace in "${LONG_TRACES[@]}"; do
        if [[ "$file_basename" == *"$long_trace" ]]; then
            return 0
        fi
    done
    return 1
}

is_short_trace() {
    local file_basename=$1
    for short_trace in "${SHORT_TRACES[@]}"; do
        if [[ "$file_basename" == *"$short_trace" ]]; then
            return 1
        fi
    done
    return 0
}

for FILE in "$INPUT_DIR"/*; do
    BASENAME=$(basename "$FILE")

    if ! is_short_trace "$BASENAME"; then
        continue
    fi

    for ((i = 0 ; i < 3 ; i++)); do
        echo "Processing: $BASENAME" >> "$OUTPUT_FILE"
    
        timeout $TIME_LIMIT time -v "$PROGRAM" -f "$FILE" >> "$OUTPUT_FILE" 2>&1
        EXIT_CODE=$?

        if [ $EXIT_CODE -eq 124 ]; then
            echo "The program timed out for file: $FILE" >> "$OUTPUT_FILE"
        elif [ $EXIT_CODE -ne 0 ]; then
            echo "The program encountered an error (exit code: $EXIT_CODE) for file: $FILE" >> "$OUTPUT_FILE"
        fi
        
        echo "----------------------------------------------------" >> "$OUTPUT_FILE"
    done
done

echo "Execution completed. Output is stored in $OUTPUT_FILE."
