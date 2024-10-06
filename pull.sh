#!/bin/bash
#
#SBATCH --cpus-per-task=8
#SBATCH --time=10:00
#SBATCH --mem=2G
#SBATCH --partition=slow

srun python /home/bsb10/Downloads/cmpt431_A3/scripts/page_rank_tester.pyc --execPath=/home/bsb10/Downloads/cmpt431_A3/ --scriptPath=/home/bsb10/Downloads/cmpt431_A3/scripts/page_rank_evaluator.pyc --inputPath=/home/bsb10/Downloads/cmpt431_A3/input_graphs/