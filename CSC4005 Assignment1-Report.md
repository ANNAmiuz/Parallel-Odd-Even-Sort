# CSC4005 Assignment1-Report

Student ID: 119010114  

Student Name: Huang Yingyi



[TOC]

















## How to compile & execute?

#### File Tree

```
--assignment1-119010114 #path_to_project
--report.pdf
```



```shell
# it is too slow to build under /pvfsmnt, you can choose to compile it under any directory, but you need to copy the programs and shared library under /pvfsmnt to execute
cd /path_to_project/
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j4

# copy to /pvfsmnt
# <test_program>: main & gtest_sort
cp -r /path_to_project/build/libodd-even-sort.so /some_path_under_pvfsmnt
cp -r /path_to_project/build/<test_program> /some_path_under_pvfsmnt
cd /some_path_under_pvfsmnt

#acquire multi-core resource by salloc or sbatch
#to run main
export LD_LIBRARY_PATH=<path of shared library libodd-even-sort.so>
m./main <datasize>
#to run goolge test
export LD_LIBRARY_PATH=<path of shared library libodd-even-sort.so>
mpirun ./gtest_sort
```

Result of main:

​	Sort $datasize$ of elements with MPI_sort and standard CPP sort. Compare the results. If they are equal, the test passed. Execution time will be displayed.

Result of google sort:

​	Sort 48000 randomly generated elements. The output will be examined by the program. The correctness result and execution time will be displayed in outputs.





## 1. Introduction

​	In this assignment, we implemented a parallel version of odd-even transposition sort, which can be executed on the campus cluster (a cluster that provides multiple nodes for program running). The code is implemented in a robust and correct way, which functions well on sorting the ***demand data type (int64_t)***. ***And two different implementations (difference on the choice of MPI functions in order to pass data from root process to all processes) are implemented for performance analysis.*** Then we test the performance of codes under different conditions (sorting data size and nodes for running), and analyse the results. The performance is evaluated by the execution time and the parallel speedup over sequential version. 

​	In the second part, the ***implementation method*** of the parallel odd-even transposition sort is illustrated. Some implementation ***details*** are explained, and a ***flow chart*** of the steps for sorting algorithm is provided.   To pass data from root process to all processes, scatter() and broadcast() method are used. The implementation using ***scatter*** is written in namespace ***sort1***, and the implementation using broadcast is written in namespace ***sort***. For the main program and google test program, we submitted the version under namespace ***sort***.

​	In the third part, we give the results of ***performance analysis***. For each version of implementation (***Scatter & Broadcast***), the execution time and the parallel speedup are recorded in ***tables***. To provide a clear view, we integrate the data in tables into many statistical ***graphs***. And some explanations for the performance differences are provided. In the explanations, we <u>*analyse the relations between data size and speedup on a specific core configuration, the reason for super-linear speedup, and the performance comparison between Scatter and Broadcast.*</u>

​	In the fourth part, we provide the ***conclusion*** of this assignment. At the last fifth part, there is the **reference**. 





## 2. Method



#### Who is to receive the input data

*<u>Input to one</u> instead of input to all:*

​	The program is designed to ***get the input sorting data only in the processor ranked 0*** following the conventions. It is to reduce the IO overhead. Otherwise, all processors should get a copy of whole sorting array through IO, which will be a unecessary burden for the devices. 



#### How to pass the data from process ranked 0 to all processes

​	There are two methods to pass the sorting data from processors ranked 0 to the other processors. 

​	The first is to ***broadcast*** the whole sorting array by MPI_Bcast() function, which makes all ranks get an ***individual copy of the whole array*** in sorting task. 

```c++
global_start_idx = displs[rank];
localsize = scounts[rank];
global_array = (Element *)malloc(gsize * sizeof(Element));
sorting_array = (Element *)malloc(localsize * sizeof(Element));

if (rank == 0)
    std::copy(begin, begin + gsize, global_array);
    MPI_Bcast(global_array, gsize, MPI_INT64_T, 0, MPI_COMM_WORLD);
```

​	The second is to ***scatter*** the whole sorting array to all ranks, which makes each of the processors keep its ***individual local array*** to sort. Considering that the sorting size may not be a multiple of the number of processors, we choose to use the MPI_Scatterv() function, which enables programmers to scatter different size of data to each processors.

```c++
global_start_idx = displs[rank];
localsize = scounts[rank];
sorting_array = (Element *)malloc(localsize * sizeof(Element));
MPI_Scatterv(begin, scounts, displs, MPI_INT64_T, sorting_array, localsize, MPI_INT64_T, 0, MPI_COMM_WORLD);
```

​	<u>***Both of scatter and broadcast method will be tested, although the MPI guidance suggests a philosophy that scatter should be faster than broadcast.***</u>



#### How to distribute workload to each process

​	We decide distribute the workload simply by ***division***. 

​		$W_p$: workload (data size) per core.

​		$W_t$: total workload (the size of global sorting array).

​		$n$: the number of processors (maximum as 128 in our tests).

​		$r$: the remainder after division.

Then the distributing method is:

​		$W_p = W_t/n$

And the remainder is:

​		$r = W_t \mod n$​​​​

​	If the workload (data size) **can be equally distributed** ($r=0$)​, the workload of each processor will be $W_p$​​.

​	If the workload (data size) **cannot be equally distributed** over all processors, the processor ranked 0 should take more work to do than the other processors. We decide that the processor ranked 0 should take $W_p+r$ workload. 

​	There are some ***drawbacks*** of this distribution implementation, which are acceptable in the actual execution. One of the drawbacks of this distribution is that, when $n>W_t$, only the processor ranked 0 has workload and the other processors idle, which means the ***parallel sort becomes sequential***. However, in our tests (which will be displayed in detail in the next part), when $W_t<128$, which is the maximum value of the number of processors, the sequential execution time ($n = 1$) will always be less than 1ms (to short to be detected). Under this situation ($W_t<128$), the sequential version of sorting performs well, while distributing $W_t$ to more than one processors will produce unnecessary overhead due to message passing. Another drawback of this distribution is that the ***workload is not the mostly balanced***. However, since $r<n$​​, such imbalance is acceptable. Especially when $W_t$ is large, the workload ($W_p+r$​) on processor ranked 0 could be viewed to be balanced with workload ($W_p$​) of the other processors.



#### How to simulate the sequential odd-even transposition sort into a parallel version

​	In the ***sequential version*** of odd-even transposition sort, there is one process and one sorting array. At first, iterate the array, compare the data of odd index and its previous data, swap them into the correct positions (ascending), and do the next iteration. In the next iteration, compare the data of even index and its previous data, swap them into the correct position (ascending), and do the next iteration. Assume there are $n$ data to sort. After $n$ such iterations, the array must be sorted.

​	In the ***parallel version*** of odd-even transposition sort, we *<u>split the global sorting array, scatter to each of the processes, let them sort their local array, and pass / receive elements from other processors</u>*. To simulate the sequential version of odd-even transposition sort, comparison and swapping are necessary. Each process should do the comparison and swapping on the local array. It may also need to do message passing with its previous rank and next rank. For example, if in the current loop, the program should compare the element of odd index with its previous element. And we focus on the process ranked $n$​. For $n$​, if the first element of the local array is of odd global index, it should send this element to process ranked $n-1$​​ (if exists), letting $n-1$​ do the comparison and wait to receive element from $n-1$​. Besides, if the first element of process ranked $n+1$​'s local array is of odd global index, it should receive that element, compare it with its last element of local array, keep the smaller one, and send the larger one to $n+1$​​​ (if exists).

​	Therefore, to simulate the sequential odd-even transposition sort into a parallel version, we should ***(1) coordinate local sort and message passing in each process***, and ***(2) make sure that the operations for new loop $l+1$ do not interrupt the operations for old loop $l$​.*** 



#### How to coordinate local sort and message passing

​	***In each loop, each process performs the iteration on the local array***. Take process ranked $n$ as an example. If the current loop is for odd index, the iteration starts from the first element of odd global index. Otherwise, the iteration starts from the first element of even global index. 

​	Based on the global index and local index information (gained at the scattering step), the process can ***determine whether it should send the first element to process ranked $n-1$ and receive element from process ranked $n+1$​.*** 

​	Now we can start the iteration on local array $A$​. If it should send $A[0]$​ to process ranked $n-1$​​, it should send it​ in an **unblocking** manner (***latency hidding***). After the nonblocking sending, the iteration continues, performing comparing and swapping on target element.

​	After the iteration, if it should receive an element from process ranked $n+1$​, it should call receive() function to get $element_{n+1}$​. Compare $element_{n+1}$ with $A[localsize-1]$, keep the smaller at the tail of local array, and send the larger one to $n+1$ in a **unblocking** manner.

​	After that, if it had sent element to process $n-1$, it should receive an element from $n-1$​ and put it at $A[0]$. 

​	Notice that there is ***always a blocking receive matching the non-blocking send***, there is no need to wait for the non-blocking send to finish.

```c++
for (loop = 0; loop < gsize; loop++)
{
    recv_from_next = 0;
    send_to_prev = 0;

    cur_idx = (global_start_idx & 1) ^ oddturn;
    if ((global_start_idx + localsize) < gsize)
        recv_from_next = !(((global_start_idx + localsize) & 1) ^ oddturn);
    if (cur_idx == 0)
    {
        cur_idx += 2;
        num_to_prev = sorting_array[0];
        if (rank != 0)
        {
            MPI_Isend(&num_to_prev, 1, MPI_INT64_T, rank - 1, loop, MPI_COMM_WORLD, &request1);
            send_to_prev = 1;
        }
    }
    for (; cur_idx < localsize; cur_idx += 2)
    {
        if (sorting_array[cur_idx] < sorting_array[cur_idx - 1])
            std::swap(sorting_array[cur_idx], sorting_array[cur_idx - 1]);
    }
    if (recv_from_next)
    {
        MPI_Recv(&num_from_next, 1, MPI_INT64_T, rank + 1, loop, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        num_to_next = num_from_next;
        if (num_from_next < sorting_array[localsize - 1])
        {
            num_to_next = sorting_array[localsize - 1];
            sorting_array[localsize - 1] = num_from_next;
        }
        MPI_Isend(&num_to_next, 1, MPI_INT64_T, rank + 1, loop, MPI_COMM_WORLD, &request2);
    }
    if (send_to_prev)
    {
        MPI_Recv(&num_from_prev, 1, MPI_INT64_T, rank - 1, loop, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        sorting_array[0] = num_from_prev;
    }

    oddturn = !oddturn;
}
```

​	

#### How to avoid conflicts between loops

​	There may be conflicts between loops. For example, in the odd loop, it may receive the element sent in even loop and mistakenly use it for odd swap. Such conflicts will provide the incorrect sorting results. There are ***two methods*** to avoid conflicts between different loops. 

​	The first method is to ***add a barrier*** at the end of the each loop. It forces all processes to finish the local sorting and message passing of this loop before getting into the next loop. However, it may drag down some of the processes, since all the processes should wait for the slowest one to continue working.

​	The second method is to add a tag to the send and receive function. The simplest way is to **take the current loop number as tag**. The second method is better than the first one since processes are forced to wait only if message passing instead to barrier. Therefore, we ***adopt the second method***.

​	

#### How to gather data from all ranks to rank0

​	Use ***MPI_Gatherv() to collect all the local sorting array*** to root process ranked 0.



####  Flow Chart

​	The program method can be illustrated by the following flow chart. The steps such as inputting data to process ranked 0 is eliminated. ***Before the flow chart start, the whole sorting data array has already been passed to process ranked 0***. Then the steps of this flow chart execute sequentially.



**NOTICE:** 

​	We have implemented ***two versions*** of sorting using MPI_Scatterv() and MPI_Bcast() to distribute the global array separately. The flow chart is for the first version: scatter. For the second version: broadcast, the flow chart is similar, while ***the blocks with a "(change)" in description are a little bit different***.

<img src="CSC4005 Assignment1-Report.assets/image-20211011152246630.png" alt="image-20211011152246630" style="zoom:150%;" />



​	To provide a clear view, the procedure of ***doing loops* *is hidden*** in the flow chart above. The details of *doing loops* is illustrated in the next flow chart.

<img src="CSC4005 Assignment1-Report.assets/image-20211011152343604-16339370264851.png" alt="image-20211011152343604" style="zoom:150%;" />









## 3. Result

#### Function

​	We used the ***provided google test*** program to test the function of our sorting program. In the google test, test data is generated randomly, where we can ***specify the data size***. Then it does the C++ standard sort and our MPI parallel odd-even transposition sort, compare the two sorting results, and display the correctness of our sorting method by showing the equality of these two results. 

​	Here is one of the outputted test results. The well functioning of our odd-even transposition sort is indicated by the *OK* words, and the performance (execution time) is displayed in millisecond (ms).

![image-20211010175757531](CSC4005 Assignment1-Report.assets/image-20211010175757531.png)

​	Our odd-even sort passed all of the tests. Therefore, it functions correctly in sorting task.



#### Experiments

##### Version 1: MPI_Scatterv()

In this implementation, we use 

***The Execution Time with Different Sorting size and Number of Cores (ms)***

| Data Size\Cores | 1        | 2        | 4        | 8     | 16    | 32    | 64    | 96   | 128  |
| --------------- | -------- | -------- | -------- | ----- | ----- | ----- | ----- | ---- | ---- |
| 50              | 0        | 0        | 0        | 0     | 0     | 0     | 0     | 0    | 0    |
| 100             | 0        | 0        | 0        | 0     | 0     | 0     | 0     | 0    | 0    |
| 200             | 0        | 0        | 0        | 0     | 0     | 0     | 0     | 0    | 0    |
| 500             | 0        | 0        | 0        | 0     | 0     | 0     | 1     | 1    | 1    |
| 1000            | 1        | 0        | 0        | 0     | 1     | 2     | 2     | 2    | 2    |
| 2000            | 4        | 2        | 1        | 1     | 2     | 2     | 5     | 5    | 5    |
| 4000            | 19       | 9        | 6        | 4     | 4     | 5     | 10    | 10   | 10   |
| 8000            | 82       | 40       | 27       | 15    | 9     | 7     | 20    | 21   | 19   |
| 16000           | 332      | 166      | 108      | 67    | 37    | 20    | 40    | 39   | 44   |
| 32000           | 1347     | 675      | 444      | 268   | 148   | 77    | 83    | 80   | 75   |
| 64000           | 5458     | 2747     | 1829     | 1117  | 614   | 311   | 177   | 156  | 165  |
| 128000          | 21025    | 11081    | 7419     | 4602  | 2546  | 1304  | 676   | 475  | 403  |
| 256000          | 85152    | 44682    | 29692    | 18476 | 10430 | 5571  | 2794  | 1873 | 1523 |
| 512000          | (330000) | (170000) | (130000) | 74250 | 42129 | 22357 | 11405 | 7720 | 5720 |



***The Speedup with Different Data Size***

| Data Size\Cores | 1    | 2     | 4     | 8     | 16    | 32     | 64     | 96     | 128    |
| --------------- | ---- | ----- | ----- | ----- | ----- | ------ | ------ | ------ | ------ |
| 50              | \    | \     | \     | \     | \     | \      | \      | \\     | \      |
| 100             | \    | \     | \     | \     | \     | \      | \\     | \      | \      |
| 200             | \    | \     | \     | \     | \     | \      | \      | \      | \      |
| 500             | \    | \     | \     | \     | \     | \      | \      | \      | \      |
| 1000            | \    | \     | \     | \     | \     | \      | \      | \      | \      |
| 2000            | 1    | 2.000 | 4.000 | 4.000 | 2.000 | 2.000  | 0.800  | 0.800  | 0.800  |
| 4000            | 1    | 2.111 | 3.167 | 4.750 | 4.750 | 3.800  | 1.900  | 1.900  | 1.900  |
| 8000            | 1    | 2.050 | 3.037 | 5.467 | 9.111 | 11.714 | 4.100  | 3.905  | 4.316  |
| 16000           | 1    | 2.000 | 3.074 | 4.955 | 8.973 | 16.600 | 8.300  | 8.513  | 7.545  |
| 32000           | 1    | 1.996 | 3.034 | 5.026 | 9.101 | 17.494 | 16.229 | 16.838 | 17.960 |
| 64000           | 1    | 1.987 | 2.984 | 4.886 | 8.889 | 17.550 | 30.836 | 34.987 | 33.079 |
| 128000          | 1    | 1.897 | 2.834 | 4.569 | 8.258 | 16.123 | 31.102 | 44.263 | 52.171 |
| 256000          | 1    | 1.906 | 2.868 | 4.609 | 8.164 | 15.285 | 30.477 | 45.463 | 55.911 |
| 512000          | 1    | 1.941 | 2.538 | 4.444 | 7.833 | 14.760 | 28.935 | 42.746 | 57.692 |



To illustrate the speedup over different the number of processors better, we provide ***a graph for the table above***:



<img src="CSC4005 Assignment1-Report.assets/image-20211011223448932.png" alt="image-20211011223448932"  />



##### Version 2: MPI_Bcast()

***The Execution Time with Different Sorting size and Number of Cores (ms)***

| Data Size\Cores | 1        | 2        | 4        | 8     | 16    | 32    | 64   | 96   | 128  |
| --------------- | -------- | -------- | -------- | ----- | ----- | ----- | ---- | ---- | ---- |
| 50              | 0        | 0        | 0        | 0     | 0     | 0     | 0    | 0    | 0    |
| 100             | 0        | 0        | 0        | 0     | 0     | 0     | 0    | 0    | 0    |
| 200             | 0        | 0        | 0        | 0     | 0     | 0     | 0    | 0    | 0    |
| 500             | 0        | 0        | 0        | 0     | 0     | 0     | 1    | 1    | 1    |
| 1000            | 1        | 0        | 0        | 0     | 1     | 2     | 2    | 2    | 2    |
| 2000            | 4        | 2        | 1        | 1     | 2     | 2     | 5    | 5    | 5    |
| 4000            | 19       | 9        | 6        | 4     | 4     | 5     | 10   | 10   | 10   |
| 8000            | 40       | 19       | 11       | 8     | 7     | 7     | 26   | 21   | 19   |
| 16000           | 203      | 78       | 41       | 25    | 16    | 17    | 40   | 38   | 45   |
| 32000           | 1071     | 409      | 193      | 91    | 55    | 36    | 81   | 83   | 76   |
| 64000           | 4851     | 2168     | 1308     | 464   | 186   | 113   | 162  | 157  | 167  |
| 128000          | 20825    | 9904     | 6283     | 3244  | 1062  | 391   | 360  | 365  | 333  |
| 256000          | 84992    | 42211    | 27691    | 15804 | 7637  | 2224  | 897  | 800  | 767  |
| 512000          | (330000) | (170000) | (130000) | 68894 | 36448 | 16661 | 4706 | 2410 | 2037 |



***The Speedup with Different Data Size***

| Data Size\Cores | 1    | 2     | 4     | 8          | 16         | 32         | 64         | 96          | 128         |
| --------------- | ---- | ----- | ----- | ---------- | ---------- | ---------- | ---------- | ----------- | ----------- |
| 50              | 1    | \     | \     | \          | \          | \          | \          | \           | \           |
| 100             | 1    | \     | \     | \          | \          | \          | \          | \           | \           |
| 200             | 1    | \     | \     | \          | \          | \          | \          | \           | \           |
| 500             | 1    | \     | \     | \          | \          | \          | \          | \           | \           |
| 1000            | 1    | \     | \     | \          | \          | \          | \          | \           | \           |
| 2000            | 1    | 2.000 | 4.000 | 4.000      | 2.000      | 2.000      | 0.800      | 0.800       | 0.800       |
| 4000            | 1    | 2.111 | 3.167 | 4.750      | 4.750      | 3.800      | 1.900      | 1.900       | 1.900       |
| 8000            | 1    | 2.105 | 3.636 | 5.000      | 5.714      | 5.714      | 1.538      | 1.905       | 2.105       |
| 16000           | 1    | 2.603 | 4.951 | 8.120      | 12.688     | 11.941     | 5.075      | 5.342       | 4.511       |
| 32000           | 1    | 2.619 | 5.549 | **11.769** | **19.473** | **29.750** | 13.222     | 12.904      | 14.092      |
| 64000           | 1    | 2.238 | 3.709 | **10.455** | **26.081** | **42.929** | 29.944     | 30.898      | 29.048      |
| 128000          | 1    | 2.103 | 3.314 | 6.420      | **19.609** | **53.261** | 57.847     | 57.055      | 62.538      |
| 256000          | 1    | 2.014 | 3.069 | 5.378      | 11.129     | **38.216** | **94.751** | **106.240** | 110.811     |
| 512000          | 1    | 1.941 | 2.538 | 4.790      | 9.054      | 19.807     | **70.123** | **136.929** | **162.003** |



To illustrate the speedup over different the number of processors better, we provide ***a graph for the table above***:



![image-20211011224122329](CSC4005 Assignment1-Report.assets/image-20211011224122329.png)



*The test data is generated randomly and the execution time is recorded by the **provided Google test**, where the data is of type **INT64_T**. There are two categories of tests in the google test program, among which the results of second test "odd-even test. random" are used here.* 

*In this test, a specific amount of random data is generated, and the execution time is calculated by recording the sorting start time and end time and doing "end-start".*

NOTICE: When the execution time exceeds the time limit, the program will be canceled. The data of these programs are tagged in the **()**.Therefore, all the time data exceeding the time limit is **predicted** but not tested based on the time complexity $O(n^2)$​ and the  previous performance (when the data size is twice larger, the execution time is about fourth time longer).   



##### Comparison Between Version 1 & Version 2



​	To compare the performance between version 1 and version 2, we derive the following two graphs:

**Difference on Execution Time (ms)**

(Version 1 - Version 2)

| Data Size\Cores | 1    | 2    | 4    | 8    | 16   | 32   | 64   | 96   | 128  |
| --------------- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- | ---- |
| 50              | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 100             | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 200             | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 500             | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 1000            | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 2000            | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 4000            | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    | 0    |
| 8000            | 42   | 21   | 16   | 7    | 2    | 0    | -6   | 0    | 0    |
| 16000           | 129  | 88   | 67   | 42   | 21   | 3    | 0    | 1    | -1   |
| 32000           | 276  | 266  | 251  | 177  | 93   | 41   | 2    | -3   | -1   |
| 64000           | 607  | 579  | 521  | 653  | 428  | 198  | 15   | -1   | -2   |
| 128000          | 200  | 1177 | 1136 | 1358 | 1484 | 913  | 316  | 110  | 70   |
| 256000          | 160  | 2471 | 2001 | 2672 | 2793 | 3347 | 1897 | 1073 | 756  |
| 512000          | 0    | 0    | 0    | 5356 | 5681 | 5696 | 6699 | 5310 | 3683 |



**Difference on Speedup**

(Version 2 - Version 1)

| Data Size\Cores | 1    | 2     | 4     | 8      | 16     | 32     | 64     | 96     | 128     |
| --------------- | ---- | ----- | ----- | ------ | ------ | ------ | ------ | ------ | ------- |
| 50              | 0    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 100             | 0    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 200             | 0    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 500             | 0    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 1000            | 0    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 2000            | 1    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 4000            | 1    | 0     | 0     | 0      | 0      | 0      | 0      | 0      | 0       |
| 8000            | 1    | 0.055 | 0.599 | -0.467 | -3.397 | -6     | -2.562 | -2     | -2.211  |
| 16000           | 1    | 0.603 | 1.877 | 3.165  | 3.715  | -4.659 | -3.225 | -3.171 | -3.034  |
| 32000           | 1    | 0.623 | 2.515 | 6.743  | 10.372 | 12.256 | -3.007 | -3.934 | -3.868  |
| 64000           | 1    | 0.251 | 0.725 | 5.569  | 17.192 | 25.379 | -0.892 | -4.089 | -4.031  |
| 128000          | 1    | 0.206 | 0.48  | 1.851  | 11.351 | 37.138 | 26.745 | 12.792 | 10.367  |
| 256000          | 1    | 0.108 | 0.201 | 0.769  | 2.965  | 22.931 | 64.274 | 60.777 | 54.9    |
| 512000          | 1    | 0     | 0     | 0.346  | 1.221  | 5.047  | 41.188 | 94.183 | 104.311 |

To illustrate the execution time and speedup differences better, we provide ***graphs for the tables above***:

![image-20211011233136350](CSC4005 Assignment1-Report.assets/image-20211011233136350.png)

![image-20211011224206687](CSC4005 Assignment1-Report.assets/image-20211011224206687.png)



#### Performance Analysis



##### Analysis on Relationship Between Data Size and Speedup

​	Review the experiment results above:



<img src="CSC4005 Assignment1-Report.assets/image-20211011223448932.png" alt="image-20211011223448932"  />

![image-20211011224122329](CSC4005 Assignment1-Report.assets/image-20211011224122329.png)



​	These two graphs show the ***speedup achieved by different number of processors on data size from 50 to 512000***. The first graph is the result of the program version 1 (scatter). The second is result of the program version 2 (broadcast).

​	It can be easily derived that the ***data size affects the speedup*** achieved by a certain number of processors. For these two implementation, when the data size is less than 1000, speedup achieved by multiple processors are undetectable (ms). When the data size gets larger, the speedup is more significant. However, when the data size is large enough to observe a speedup $S_n$​​ achieved by $n$​​ processors, the speedup $S_n$​​ cannot stay constant. A certain number of processors may best accelerate the execution on a specific data size. ***Assume that $n$​​ processors accelerates the program best on the data size $D_n$​​.*** ***The larger $n$​​ is, the larger $D_n$​​ is***. For these two implementations, $D_2$​​, $D_4$​​, and $D_8$​​ are on the range $10^3$​​~$5 \times 10^4$​​; $D_{16}$​​, and $D_{32}$​​, and  are on the range $5 \times 10^4$​​ ~  $25 \times 10^4$​; $D_{64}$​​​ is on range $25 \times 10^4$ ~ $50 \times 10^4$; $D_{96}$ and $D_{128}$​ are on the larger than $50 \times 10^4$​. 

​	The ***reasons for the impacts of data size on speedup $S_{n}$ achieved by $n$​​ processors*** are uncertain. Here are one of the possibility. 

​	For $n$ processors, when the ***data size is less than*** $D_n$, the local workload for each processor is too small compared with the communication IO overhead. It will be more economic to distribute more local workload but decrease the number of processors to decrease the cost on message passing. When the ***data size is larger than*** $D_{n}$​​​​, the local workload may be too large. Memory overhead becomes too high since memory exchanges (page swap, cache exchange...) between different storage devices take place more frequently (due to the limited cache and physical memory storage). The local IO overhead becomes the bottleneck. 

​	Therefore, it is important to ***choose the number of processors based on the sorting data size***. If the number of processors is too large, the parallel resource cannot be fully utilized. It is possible to decrease the number of processors to achieve a better speedup (example: $S_{32}$​ is larger than $S_{64}$​​​ on some data size). If the number of processor is too small, the local workload will be too high. It is possible to increase the number of processors to achieve a better speedup (example: When data size is 512000, speedup increases as the number of processors increases).



##### Analysis on the Super-linear Speedup



​	Review the performance test results above. With the version 2 implementation, where we used the ***Broadcast*** to pass data from root process to all processes, we achieved super-linear speedup on some cores configurations on some data size.



***Speedup of version 2 implementation (Super-linear Speedup is bolded)***



| Data Size\Cores | 1    | 2     | 4     | 8          | 16         | 32         | 64         | 96          | 128         |
| --------------- | ---- | ----- | ----- | ---------- | ---------- | ---------- | ---------- | ----------- | ----------- |
| 32000           | 1    | 2.619 | 5.549 | **11.769** | **19.473** | 29.750     | 13.222     | 12.904      | 14.092      |
| 64000           | 1    | 2.238 | 3.709 | **10.455** | **26.081** | **42.929** | 29.944     | 30.898      | 29.048      |
| 128000          | 1    | 2.103 | 3.314 | 6.420      | **19.609** | **53.261** | 57.847     | 57.055      | 62.538      |
| 256000          | 1    | 2.014 | 3.069 | 5.378      | 11.129     | **38.216** | **94.751** | **106.240** | 110.811     |
| 512000          | 1    | 1.941 | 2.538 | 4.790      | 9.054      | 19.807     | **70.123** | **136.929** | **162.003** |



*"More tests are added to get the super-linear speedup for $n<128$​"*

| Data Size\Cores     | 1                 | 64    | 96          | 128         |
| ------------------- | ----------------- | ----- | ----------- | ----------- |
| 768000 (time / ms)  | $7.5 \times 10^5$ | 17032 | 7169        | 4196        |
| 768000 (speedup)    | 1                 | 43.6  | **103.571** | **176.954** |
| 1024000 (time / ms) | $1.4 \times 10^6$​ |       | 18688       | 9664        |
| 1024000 (speedup)   | 1                 |       | 70.6        | **137**     |
| 2048000 (time / ms) | $5.3 \times 10^6$ |       |             | 70884       |
| 2048000 (speedup)   |                   |       |             | 75          |



![image-20211011224122329](CSC4005 Assignment1-Report.assets/image-20211011224122329.png)

​	

​	There are two main reasons for the super-linear speedup. The first reason is that the parallelism exposing the problem under a better algorithm. The second reason is that the parallelism provides smaller local dataset to each process. In our implementation of odd-even transposition sort, ***it is the second reason that contributes to the super-linear speedup.***

​	***The first reason is invalid*** for our implementation since our parallel sort keeps the comparing and swapping with neighbour as essential steps. It makes the parallel algorithm has the same time complexity in theory as $O{(n^2)}$​​​. ***The second reason is valid*** since we observed that the super-linear speedup is achieved when the local data size falls in a range. 

> When $n=8$​​, the super-linear speedup is achieved when the local data size ​​ is in $4 \times 10^3$​​ ~ $8 \times 10^3$​​. 
>
> When $n=16$​​, the super-linear speedup is achieved when the local data size is in $2 \times 10^3$​​ ~ $8 \times 10^3$​​. 
>
> When $n=32$​​, the super-linear speedup is achieved when the local data size is in $2 \times 10^3$​​ ~ $8 \times 10^3$​​. 
>
> When $n=64$​​​​​, the super-linear speedup is achieved when the local data size is larger  $4 \times 10^3$​​​ ~ $8 \times 10^3$​​​​​​. 
>
> When $n=96$, the super-linear speedup is achieved when the local data size is larger than $3 \times 10^3$ ~ $8 \times 10^3$. 
>
> When $n=128$, the super-linear speedup is achieved when the local data size is larger than $4 \times 10^3$ ~ $8 \times 10^3$. 

We can ***conclude that the local data size $D_{super}$ contributing to the super-linear speedup is in range $2 \times 10^3$ ~ $8 \times 10^3$.***



##### Performance Comparison of Broadcast and Scatter



![image-20211011233128734](CSC4005 Assignment1-Report.assets/image-20211011233128734.png)

(Execution Time of Version 1 - Execution Time of Version 2)



![image-20211011225837417](CSC4005 Assignment1-Report.assets/image-20211011225837417.png)

(Speedup of Version 2 - Speedup of Version 1)



​	Reviewing the results we gained in the previous part, we can find that the version using ***Scatter*** is slower than the version using ***Broadcast***. When the data size is getting larger, such execution time difference is getting larger. And the version using ***Scatter*** can achieve higher speedup than the version using ***Broadcast*** on the same configuration. Since the only difference between the two versions locates in the way passing data from root process to all processes, we can conclude that the performance difference lies on the efficiency difference between MPI_Bcast() and MPI_Scatterv().

​	Although the philosophy of Message Passing method suggests that *<u>scatter should be faster than broadcast</u>*, the actual implementations of many message passing libraries lead to the opposite result. Some researchers doing benchmark tests on MPI performance have indicated that some of the actual implementations of broadcast are faster than that of scatter (Hunoldet al., 2016). They emphasized the a guidance for MPI that:

> MPI_Scatter should not be slower than MPI_Bcast. The reason is that the semantics of MPI_Scatter can be implemented using MPI_Bcast, by broadcasting the entire vector before processes take their share depending on their rank.

In their benchmark tests, some libraries (implementations) violates this guidance. However, their tests are made on Jupiter, while the implementations of open MPI may not be the same for C++ and Jupiter. However, we can still conclude that MPI_Bcast is faster than MPI_Scatterv in our implementation.





## 4. Conclusion

​	We implemented **two versions of parallel odd-even transposition sort** for ***int64_t*** data. The first version under namespace  *sort1* uses MPI_Scatter to pass data from root process to all processes. The second version under namespace uses MPI_Bcast to pass data from root process to all processes. We **tested** the program on $1,2,4,8,16,32,64,96$ and $128$​​​ cores, recorded the execution, and analyse the performance and speedup. In the analysis, we found the **relation between data size and speedup**, suggesting that for each data size, there will be a best core configuration (the number of cores). We also analysed the **reason for super-linear speedup**, concluding that there is a range for local data set, which contributing to the super-linear speedup. At last, we found that the efficiency of MPI_Bcast is better than the one of MPI_Scatterv, which is **a violation of the MPI guidance**.



## 5. Reference

Hunold, Sascha & Carpen-Amarie, Alexandra & Lübbe, Felix & Träff, Jesper. (2016). PGMPI: Automatically Verifying Self-Consistent MPI Performance Guidelines. 

