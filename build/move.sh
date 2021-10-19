cmake --build . -j4
rm /pvfsmnt/119010114/csc4005-assignment-1/build/main
rm /pvfsmnt/119010114/csc4005-assignment-1/build/libodd-even-sort.so
rm /pvfsmnt/119010114/csc4005-assignment-1/build/input
rm /pvfsmnt/119010114/csc4005-assignment-1/build/gtest_sort
rm /pvfsmnt/119010114/csc4005-assignment-1/build/test.sh
cp /home/119010114/csc4005-assignment-1/build/main /home/119010114/csc4005-assignment-1/build/libodd-even-sort.so /home/119010114/csc4005-assignment-1/build/input /home/119010114/csc4005-assignment-1/build/gtest_sort /home/119010114/csc4005-assignment-1/build/test.sh -d /pvfsmnt/119010114/csc4005-assignment-1/build/