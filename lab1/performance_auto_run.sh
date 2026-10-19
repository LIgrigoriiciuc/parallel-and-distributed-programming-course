#!/bin/bash

for size in "1000 500" "20000 10000"; do
  for threads in 1 4 8; do
    for mode in fine global_lock; do
      ops=$((800000 / threads))
      ./main.out $threads $ops $size $mode
    done
  done
done