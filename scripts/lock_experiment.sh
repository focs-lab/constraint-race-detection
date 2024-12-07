#!/bin/bash

result_file="results.txt"

> $result_file

for i in {1..15}
do
    python3 scripts/lock_experiment.py $i
    make gen_single_trace file=lock_experiment
    ./bin/predictor -f traces/formatted_traces/lock_experiment -c 1 >> $result_file
    echo "---------" >> $result_file
done