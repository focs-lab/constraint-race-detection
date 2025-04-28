#!/bin/bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BENCHMARK_TRACES_DIR="$PROJECT_DIR/traces"
CORRECTNESS_TRACES_DIR="$PROJECT_DIR/correctness"
PROGRAM="$PROJECT_DIR/build/predictor"
VERIFIER="$PROJECT_DIR/build/verifier"

TIMEOUT=300

# Parse command line arguments
TRACE_TYPE="benchmark"
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            TRACE_TYPE="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-t|--type benchmark|correctness]"
            exit 1
            ;;
    esac
done

if [[ "$TRACE_TYPE" != "benchmark" && "$TRACE_TYPE" != "correctness" ]]; then
    echo "Error: trace type must be either 'benchmark' or 'correctness'"
    echo "Usage: $0 [-t|--type benchmark|correctness]"
    exit 1
fi

if [ ! -f "$PROGRAM" ]; then
    echo "Error: $PROGRAM not found"
    exit 1
fi

BENCHMARK_TRACES=(
    # "cryptorsa" 
    # "linkedlist" 
    # "lusearch" 
    # "xalan" 
    # "bufwriter" 
    # "moldyn" 
    "readerswriters" 
    # "ftpserver" 
    # "derby" 
    "jigsaw"
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

CORRECTNESS_TRACES=(
    "sample1.txt"
    "sample2.txt"
    "sample3.txt"
    "sample4.txt"
    "sample5.txt"
)

PREDICTOR_EXEC="$PROGRAM --model casual -c 1 -f"
VERIFIER_EXEC="$VERIFIER -f"

# Select traces array and directory based on type
if [ "$TRACE_TYPE" = "benchmark" ]; then
    TRACES=("${BENCHMARK_TRACES[@]}")
    TRACES_DIR="$BENCHMARK_TRACES_DIR"
else
    TRACES=("${CORRECTNESS_TRACES[@]}")
    TRACES_DIR="$CORRECTNESS_TRACES_DIR"
fi

for trace in "${TRACES[@]}"; do
    if [ ! -f "$TRACES_DIR/$trace" ]; then
        echo "Error: $trace not found"
        continue
    fi

    echo "Running $trace"
    gtimeout $TIMEOUT $PREDICTOR_EXEC "$TRACES_DIR/$trace" 2>&1
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 124 ]; then
        echo "Error: $trace timed out"
        continue
    elif [ $EXIT_CODE -ne 0 ]; then
        echo "Error: $trace failed with exit code $EXIT_CODE"
        continue
    fi
done