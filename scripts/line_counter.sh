#!/bin/bash

SUBDIR="traces/human_readable_traces"
OUTPUT_FILE="output/line_count.txt"
> "$OUTPUT_FILE"

for FILE in "$SUBDIR"/*.txt
do
  LINE_COUNT=$(wc -l < "$FILE")
  FILENAME=$(basename "$FILE")
  echo "$FILENAME - $LINE_COUNT" >> "$OUTPUT_FILE"
done

echo "Line counts written to $OUTPUT_FILE"
