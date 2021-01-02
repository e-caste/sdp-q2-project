i#!/bin/bash

./run.sh --download
./run.sh --generate

[[ ! -d logs ]] && mkdir logs

for i in {1..10}; do
  ./run.sh --run --labels "$i" | tee "log_$(date +%Y-%m-%d_%H-%M-%S)_mode=all_labels=$i.txt"
done