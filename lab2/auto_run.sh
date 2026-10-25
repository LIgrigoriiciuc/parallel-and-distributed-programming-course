#!/bin/bash
for q in 1 2 5 10 50 100 500 1000 5000 10000 100000; do
    for el in 1 2 5 10 50 100 500 1000 5000 10000; do
        for run in _ _ _; do
            ./main.out $el $q 69
        done
    done
done