#!/bin/bash
#SBATCH --account=csc4005
#SBATCH --partition=debug
#SBATCH --qos=normal
#SBATCH --ntasks=1
#SBATCH --nodes=1
echo"mainmode: " && /bin/hostname
mpirun /pvfsmnt/119010114/csc4005-assignment-1/build/gtest_sort
