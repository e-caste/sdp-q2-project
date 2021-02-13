#!/bin/bash

./run.sh --download
./run.sh --generate

[[ ! -d logs ]] && mkdir logs

# only roots search times
./run.sh --run --labels 0 | tee logs/"log_$(date +%Y-%m-%d_%H-%M-%S)_mode=all_labels=0.txt"

for t in 1 2 8 32; do
  for l in {1..10}; do
    ./run.sh --run --labels "$l" --threads "$t" benchmark | tee logs/log_"$(date +%Y-%m-%d_%H-%M-%S)"_mode=benchmark_labels="$l"_threads="$t".txt
    ./run.sh --run --labels "$l" --threads "$t" test | tee logs/log_"$(date +%Y-%m-%d_%H-%M-%S)"_mode=test_labels="$l"_threads="$t".txt
  done
done