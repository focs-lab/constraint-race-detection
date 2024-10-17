#!/bin/bash

INPUT_DIR="$PWD/traces/STD-Format"
RAPID_DIR=$HOME/rapid

if [ -z "$1" ]; then
  echo "Usage: $0 <engine>"
  exit 1
fi

ENGINE=$1

OUTPUT_FILE="$PWD/output/rapid_results_$ENGINE.txt"

> "$OUTPUT_FILE"

echo "Running with $ENGINE engine" | tee -a "$OUTPUT_FILE"
echo "####################################################" | tee -a "$OUTPUT_FILE"
echo "" | tee -a "$OUTPUT_FILE"

for std_file in "$INPUT_DIR"/*.std; do
  if [ -f "$std_file" ]; then
    echo "Processing $std_file" | tee -a "$OUTPUT_FILE"
    (
      cd "$RAPID_DIR" || exit
      java -cp rapid.jar:./lib/*:./lib/jgrapht/* "$ENGINE" -f std -p "$std_file" | tee -a "$OUTPUT_FILE"
    )
    echo "----------------------------------------------------" | tee -a "$OUTPUT_FILE"
  else
    echo "No .std files found in $INPUT_DIR"
  fi
done
