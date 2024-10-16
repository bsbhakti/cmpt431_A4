#!/bin/bash
#
#SBATCH --cpus-per-task=8
#SBATCH --time=10:00
#SBATCH --mem=2G
#SBATCH --partition=slow

srun ./page_rank_pull_parallel --nThreads=4 --nIterations=20 --strategy=4 --granularity=100 --inputFile="/scratch/input_graphs/test_25M_50M"
