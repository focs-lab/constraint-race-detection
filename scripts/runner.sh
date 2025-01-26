#!/bin/bash

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
TRACES_DIR="$PROJECT_DIR/traces"
PROGRAM="$PROJECT_DIR/build/predictor"
VERIFIER="$PROJECT_DIR/build/verifier"

TIMEOUT=600

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
    # "readerswriters" 
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

PREDICTOR_EXEC="$PROGRAM --log-binary-witness -f"
VERIFIER_EXEC="$VERIFIER -f"

for trace in "${BENCHMARK_TRACES[@]}"; do

    if [ ! -f "$TRACES_DIR/$trace" ]; then
        echo "Error: $trace not found"
        continue
    fi

    echo "Running $trace"
    gtimeout $TIMEOUT $VERIFIER_EXEC "$TRACES_DIR/$trace" 2>&1
    EXIT_CODE=$?

    if [ $EXIT_CODE -eq 124 ]; then
        echo "Error: $trace timed out"
        continue
    elif [ $EXIT_CODE -ne 0 ]; then
        echo "Error: $trace failed with exit code $EXIT_CODE"
        continue
    fi
done