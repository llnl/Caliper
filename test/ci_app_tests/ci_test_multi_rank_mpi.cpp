// Test multi-rank MPI configs

#include "ci_test_util.hpp"

#include <caliper/cali.h>
#include <caliper/cali-mpi.h>

#include <caliper/CollectiveOutputChannel.h>

#include <mpi.h>

#include <iostream>
#include <map>
#include <sstream>
#include <tuple>
#include <vector>

int main(int argc, char* argv[])
{
    MPI_Init(&argc, &argv);

    int rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    int size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        std::cerr << "ci_test_multi_rank_mpi: Need at least 2 ranks" << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    std::shared_ptr<cali::CollectiveOutputChannel> profile_channel, spot_channel;
    std::string errmsg;

    std::tie(profile_channel, errmsg) = cali::make_collective_output_channel("runtime-profile,profile.mpi,region.count,output.format=json");
    if (!profile_channel) {
        std::cerr << "Caliper error: " << errmsg << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -2);
    }

    std::tie(spot_channel, errmsg) = cali::make_collective_output_channel("spot,profile.mpi");
    if (!spot_channel) {
        std::cerr << "Caliper error: " << errmsg << std::endl;
        MPI_Abort(MPI_COMM_WORLD, -2);
    }

    profile_channel->start();
    spot_channel->start();

    {
        CALI_CXX_MARK_FUNCTION;
        MPI_Barrier(MPI_COMM_WORLD);

        CALI_MARK_COMM_REGION_BEGIN("p2p");
        int next = rank + 1 >= size ? 0 : rank + 1;
        int prev = rank - 1 < 0 ? (size-1) : rank - 1;

        int ibuf_0 = 42, ibuf_1 = 24;
        int obuf_0 = 0,  obuf_1 = 0;

        MPI_Request req[4];

        MPI_Isend(&ibuf_0, 1, MPI_INT, prev, 0, MPI_COMM_WORLD, &req[0]);
        MPI_Isend(&ibuf_1, 1, MPI_INT, next, 1, MPI_COMM_WORLD, &req[1]);
        MPI_Irecv(&obuf_0, 1, MPI_INT, next, 0, MPI_COMM_WORLD, &req[2]);
        MPI_Irecv(&obuf_1, 1, MPI_INT, prev, 1, MPI_COMM_WORLD, &req[3]);
        MPI_Waitall(4, req, MPI_STATUSES_IGNORE);
        CALI_MARK_COMM_REGION_END("p2p");
    }

    spot_channel->stop();
    profile_channel->stop();

    std::ostringstream profile_os;
    profile_channel->collective_flush(profile_os, MPI_COMM_WORLD);

    std::ostringstream spot_os;
    spot_channel->collective_flush(spot_os, MPI_COMM_WORLD);

    const std::vector<std::map<std::string, std::string>> profile_test_targets {
        { { "path", "main/p2p/MPI_Isend" }, { "mpi.rank", "0" }, { "Calls", "2" }, { "time", ">0" } },
        { { "path", "main/p2p/MPI_Irecv" }, { "mpi.rank", "1" }, { "Calls", "2" }, { "time", ">0" } },
        { { "path", "main/p2p/MPI_Waitall" }, { "mpi.rank", "1" }, { "Calls", "1" }, { "time", ">0" } }
    };

    std::string total_calls_str = std::to_string(size * 2);

    const std::vector<std::map<std::string, std::string>> spot_test_targets {
        { { "path", "main/p2p/MPI_Isend" }, { "avg#sum#rc.count", "2" }, { "sum#sum#rc.count", total_calls_str }, { "avg#scale#sum#time.duration.ns", ">0" } },
        { { "path", "main/p2p/MPI_Irecv" }, { "avg#sum#rc.count", "2" }, { "sum#sum#rc.count", total_calls_str }, { "avg#scale#sum#time.duration.ns", ">0" } },
    };

    if (rank == 0) {
        std::cerr << "ci_test_multi_rank_mpi: Checking profile channel output .. ";
        if (find_targets_in_json_output(profile_test_targets, profile_os.str()) > 0)
            MPI_Abort(MPI_COMM_WORLD, -3);
        std::cerr << "OK\n";

        std::cerr << "ci_test_multi_rank_mpi: Checking spot channel output .. ";
        if (find_targets_in_json_output(spot_test_targets, run_query(spot_os.str(), "format json")) > 0)
            MPI_Abort(MPI_COMM_WORLD, -4);
        std::cerr << "OK\n";
    }

    MPI_Finalize();
    return 0;
}