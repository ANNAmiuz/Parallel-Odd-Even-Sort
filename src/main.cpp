#include <odd-even-sort.hpp>
#include <mpi.h>
#include <iostream>
#include <fstream>
#include <vector>

int main(int argc, char **argv)
{
    sort::Context context(argc, argv);
    int rank, wsize;
    MPI_Comm_size(MPI_COMM_WORLD, &wsize);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc < 2)
    {
        if (rank == 0)
        {
            std::cerr << "wrong arguments" << std::endl;
            std::cerr << "usage: " << argv[0] << " <DATA_SIZE> " << std::endl;
        }
        return 0;
    }

    if (rank == 0)
    {
        sort::Element element;
        std::vector<sort::Element> data;
        long size = atoi(argv[1]);
        std::srand((unsigned)time(NULL));
        for (int i = 0; i < size; i++){
            element = std::rand();
            data.push_back(element);
        }
        std::vector<sort::Element> a = data;
        std::vector<sort::Element> b = data;
        auto info = context.mpi_sort(a.data(), a.data() + a.size());
        std::sort(b.data(), b.data() + b.size());
        if (a == b){
            std::cout<<"There are "<<wsize<<" processors"<<std::endl;
            std::cout<<"Test passed with "<<size<<" elements to sort"<<std::endl;
        }
        else{
            std::cout<<"There are "<<wsize<<" processors"<<std::endl;
            std::cout<<"Test failed with "<<size<<" elements to sort"<<std::endl;
        }
        sort::Context::print_information(*info, std::cout);
    }
    else
    {
        context.mpi_sort(nullptr, nullptr);
    }
}
