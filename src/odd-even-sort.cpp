#include <odd-even-sort.hpp>
#include <mpi.h>
#include <iostream>
#include <vector>

#define DEBUGx
/* sort1: scatter the array, gatherv the array */
/* sort2: scatter the array, gatherv the array */
/* sort: broadcast the array, gatherv the array */

namespace sort1
{
    using namespace std::chrono;

    Context::Context(int &argc, char **&argv) : argc(argc), argv(argv)
    {
        MPI_Init(&argc, &argv);
    }

    Context::~Context()
    {
        MPI_Finalize();
    }

    std::unique_ptr<Information> Context::mpi_sort(Element *begin, Element *end) const
    {
        int res;
        int rank;
        int wsize;
        int gsize = 0;            //total number to sort
        int localsize = 0;        //size of the local sorting array
        int global_start_idx = 0; //start index in the global array
        int loop = 0;             //number of loop, when n = the global sorting size,the sorting finish
        Element *sorting_array;   //array of numbers assigned to sort
        std::unique_ptr<Information> information{};

        MPI_Comm_size(MPI_COMM_WORLD, &wsize);
        res = MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (MPI_SUCCESS != res)
        {
            throw std::runtime_error("failed to get MPI world rank");
        }

        if (0 == rank)
        {
            information = std::make_unique<Information>();
            information->length = end - begin;
            res = MPI_Comm_size(MPI_COMM_WORLD, &information->num_of_proc);
            if (MPI_SUCCESS != res)
            {
                throw std::runtime_error("failed to get MPI world size");
            };
            information->argc = argc;
            for (auto i = 0; i < argc; ++i)
            {
                information->argv.push_back(argv[i]);
            }
            information->start = high_resolution_clock::now();
        }

        /// now starts the main sorting procedure
        /// @todo: please modify the following code
        int num_per_proc; //total size to sort each proc except for root
        int offset = 0;
        int *scounts, *displs; //for scatterv

        if (rank == 0)
            gsize = end - begin;

        MPI_Bcast(&gsize, 1, MPI_INT, 0, MPI_COMM_WORLD); //boradcast the global size to all ranks

        scounts = (int *)malloc(wsize * sizeof(int)); //local size
        displs = (int *)malloc(wsize * sizeof(int));  //global start index

        num_per_proc = gsize / wsize;
        scounts[0] = num_per_proc + (gsize % wsize);
        displs[0] = 0;
        offset += scounts[0];
        for (auto i = 1; i < wsize; ++i)
        {
            displs[i] = offset;
            scounts[i] = num_per_proc;
            offset += num_per_proc;
        }

        global_start_idx = displs[rank];
        localsize = scounts[rank];
        sorting_array = (Element *)malloc(localsize * sizeof(Element));
        MPI_Scatterv(begin, scounts, displs, MPI_INT64_T, sorting_array, localsize, MPI_INT64_T, 0, MPI_COMM_WORLD);

#ifdef DEBUG
        for (auto j = 0; j < localsize; ++j)
        {
            std::cout << sorting_array[j] << std::endl;
        }
        std::cout << std::endl;
#endif

        /* start sorting */
        if (localsize)
        {
            bool oddturn = 1;
            int cur_idx;
            Element num_from_next, num_to_next, num_from_prev, num_to_prev;
            int recv_from_next, send_to_prev; //condition code
            MPI_Request request1, request2;
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
                    //MPI_Wait(&request1, MPI_STATUS_IGNORE);
                    MPI_Recv(&num_from_prev, 1, MPI_INT64_T, rank - 1, loop, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    sorting_array[0] = num_from_prev;
                }
                // if (recv_from_next){
                //     MPI_Wait(&request2, MPI_STATUS_IGNORE);
                // }
                oddturn = !oddturn;
            }
        }
        MPI_Gatherv(sorting_array, localsize, MPI_INT64_T, begin, scounts, displs, MPI_INT64_T, 0, MPI_COMM_WORLD);

        if (0 == rank)
        {
            information->end = high_resolution_clock::now();
        }
        return information;
    }

    std::ostream &Context::print_information(const Information &info, std::ostream &output)
    {
        auto duration = info.end - info.start;
        auto duration_count = duration_cast<nanoseconds>(duration).count();
        auto mem_size = static_cast<double>(info.length) * sizeof(Element) / 1024.0 / 1024.0 / 1024.0;
        output << "input size: " << info.length << std::endl;
        output << "proc number: " << info.num_of_proc << std::endl;
        output << "duration (ns): " << duration_count << std::endl;
        output << "throughput (gb/s): " << mem_size / static_cast<double>(duration_count) * 1'000'000'000.0
               << std::endl;
        return output;
    }
}

namespace sort
{
    using namespace std::chrono;

    Context::Context(int &argc, char **&argv) : argc(argc), argv(argv)
    {
        MPI_Init(&argc, &argv);
    }

    Context::~Context()
    {
        MPI_Finalize();
    }

    std::unique_ptr<Information> Context::mpi_sort(Element *begin, Element *end) const
    {
        int res;
        int rank;
        int wsize;
        int gsize = 0;            //total number to sort
        int localsize = 0;        //size of the local sorting array
        int global_start_idx = 0; //start index in the global array
        int loop = 0;             //number of loop, when n = the global sorting size,the sorting finish
        Element *sorting_array;   //array of numbers assigned to sort
        Element *global_array;
        std::unique_ptr<Information> information{};

        MPI_Comm_size(MPI_COMM_WORLD, &wsize);
        res = MPI_Comm_rank(MPI_COMM_WORLD, &rank);

        if (MPI_SUCCESS != res)
        {
            throw std::runtime_error("failed to get MPI world rank");
        }

        if (0 == rank)
        {
            information = std::make_unique<Information>();
            information->length = end - begin;
            res = MPI_Comm_size(MPI_COMM_WORLD, &information->num_of_proc);
            if (MPI_SUCCESS != res)
            {
                throw std::runtime_error("failed to get MPI world size");
            };
            information->argc = argc;
            for (auto i = 0; i < argc; ++i)
            {
                information->argv.push_back(argv[i]);
            }
            information->start = high_resolution_clock::now();
        }

        /// now starts the main sorting procedure
        /// @todo: please modify the following code
        int num_per_proc; //total size to sort each proc except for root
        int offset = 0;
        int *scounts, *displs; //for scatterv

        if (rank == 0)
            gsize = end - begin;

        MPI_Bcast(&gsize, 1, MPI_INT, 0, MPI_COMM_WORLD); //boradcast the global size to all ranks

        scounts = (int *)malloc(wsize * sizeof(int)); //local size
        displs = (int *)malloc(wsize * sizeof(int));  //global start index

        num_per_proc = gsize / wsize;
        scounts[0] = num_per_proc + (gsize % wsize);
        displs[0] = 0;
        offset += scounts[0];
        for (auto i = 1; i < wsize; ++i)
        {
            displs[i] = offset;
            scounts[i] = num_per_proc;
            offset += num_per_proc;
        }

        global_start_idx = displs[rank];
        localsize = scounts[rank];
        global_array = (Element *)malloc(gsize * sizeof(Element));
        sorting_array = (Element *)malloc(localsize * sizeof(Element));
        
        if (rank == 0)
            std::copy(begin, begin + gsize, global_array);
        MPI_Bcast(global_array, gsize, MPI_INT64_T, 0, MPI_COMM_WORLD);
        //MPI_Bcast(begin, gsize, MPI_INT64_T, 0, MPI_COMM_WORLD);
        sorting_array = global_array + global_start_idx;
        //sorting_array = begin + global_start_idx;
        //std::copy(global_array + global_start_idx, global_array + global_start_idx + localsize, sorting_array);

#ifdef DEBUG
        for (auto j = 0; j < localsize; ++j)
        {
            std::cout << sorting_array[j] << std::endl;
        }
        std::cout << std::endl;
#endif

        /* start sorting */
        if (localsize)
        {
            bool oddturn = 1;
            int cur_idx;
            Element num_from_next, num_to_next, num_from_prev, num_to_prev;
            int recv_from_next, send_to_prev; //condition code
            MPI_Request request1, request2;
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
                    //MPI_Wait(&request1, MPI_STATUS_IGNORE);
                    MPI_Recv(&num_from_prev, 1, MPI_INT64_T, rank - 1, loop, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                    sorting_array[0] = num_from_prev;
                }
                // if (recv_from_next){
                //     MPI_Wait(&request2, MPI_STATUS_IGNORE);
                // }
                oddturn = !oddturn;
            }
        }
        MPI_Gatherv(sorting_array, localsize, MPI_INT64_T, begin, scounts, displs, MPI_INT64_T, 0, MPI_COMM_WORLD);

        if (0 == rank)
        {
            information->end = high_resolution_clock::now();
        }
        return information;
    }

    std::ostream &Context::print_information(const Information &info, std::ostream &output)
    {
        auto duration = info.end - info.start;
        auto duration_count = duration_cast<nanoseconds>(duration).count();
        auto mem_size = static_cast<double>(info.length) * sizeof(Element) / 1024.0 / 1024.0 / 1024.0;
        output << "input size: " << info.length << std::endl;
        output << "proc number: " << info.num_of_proc << std::endl;
        output << "duration (ns): " << duration_count << std::endl;
        output << "throughput (gb/s): " << mem_size / static_cast<double>(duration_count) * 1'000'000'000.0
               << std::endl;
        return output;
    }
}
