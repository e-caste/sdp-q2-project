#!/bin/bash

./run.sh --download
./run.sh --generate

[[ ! -d logs ]] && mkdir logs

# only roots search times
./run.sh --run --labels 0 | tee "log_$(date +%Y-%m-%d_%H-%M-%S)_mode=all_labels=0.txt"

for i in {1..10}; do
  ./run.sh --run --labels "$i" benchmark | tee "log_$(date +%Y-%m-%d_%H-%M-%S)_mode=benchmark_labels=$i.txt"
  ./run.sh --run --labels "$i" test | tee "log_$(date +%Y-%m-%d_%H-%M-%S)_mode=test_labels=$i.txt"
done