#include <memory>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <ostream>
#include <thread>

// View c++ version __cplusplus
#define version_info __cplusplus

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "role_server.h"
#include "role_client.h"
#include "rome/rdma/memory_pool/memory_pool.h"
#include "rome/rdma/connection_manager/connection_manager.h"
#include "rome/logging/logging.h"
#include "protos/experiment.pb.h"
#include "rome/util/proto_util.h"
#include "google/protobuf/text_format.h"
#include "iht_ds.h"

ABSL_FLAG(std::string, experiment_params, "", "Experimental parameters");
ABSL_FLAG(bool, send_bulk, false, "If to run bulk operations. (More for benchmarking)");
ABSL_FLAG(bool, send_test, false, "If to test the functionality of the methods.");
ABSL_FLAG(bool, send_exp, false, "If to run an experiment");

#define PATH_MAX 4096

using ::rome::rdma::ConnectionManager;
using ::rome::rdma::MemoryPool;

constexpr char iphost[] = "node0";
constexpr uint16_t portNum = 18000;

using cm_type = MemoryPool::cm_type;


int main(int argc, char **argv)
{
    ROME_INIT_LOG();
    absl::ParseCommandLine(argc, argv);
    bool bulk_operations = absl::GetFlag(FLAGS_send_bulk);
    bool test_operations = absl::GetFlag(FLAGS_send_test);
    bool do_exp = absl::GetFlag(FLAGS_send_exp);
    ExperimentParams params = ExperimentParams();
    ResultProto result_proto = ResultProto();
    std::string experiment_parms = absl::GetFlag(FLAGS_experiment_params);
    bool success = google::protobuf::TextFormat::MergeFromString(experiment_parms, &params);
    ROME_ASSERT(success, "Couldn't parse protobuf");

    // Get hostname to determine who we are
    char hostname[4096];
    gethostname(hostname, 4096);

    // Start initializing a vector of peers
    volatile bool done = false;
    MemoryPool::Peer host{0, std::string(iphost), portNum};
    MemoryPool::Peer self;
    bool outside_exp = true;
    std::vector<MemoryPool::Peer> peers;
    peers.push_back(host);

    if (params.node_count() == 0)
    {
        ROME_INFO("Cannot start experiment. Node count was found to be 0");
        exit(1);
    }

    // Set values if we are host machine as well
    if (hostname[4] == '0')
    {
        self = host;
        outside_exp = false;
    }

    // Make the peer list by iterating through the node count
    for (int n = 1; n < params.node_count(); n++)
    {
        // Create the ip_peer (really just node name)
        std::string ippeer = "node";
        std::string node_id = std::to_string(n);
        ippeer.append(node_id);
        // Create the peer and add it to the list
        MemoryPool::Peer next{static_cast<uint16_t>(n), ippeer, portNum};
        peers.push_back(next);
        // Compare after 4th character to node_id
        if (strncmp(hostname + 4, node_id.c_str(), node_id.length()) == 0)
        {
            // If matching, next is self
            self = next;
            outside_exp = false;
        }
    }

    if (outside_exp)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // So we get this printed last
        ROME_INFO("Not in experiment. Shutting down");
        return 0;
    } 

    // Make a memory pool for the node to share among all client instances
    uint32_t block_size = 1 << params.region_size();
    MemoryPool pool = MemoryPool(self, std::make_unique<MemoryPool::cm_type>(self.id));

    for (int i = 0; i < peers.size(); i++)
    {
        ROME_INFO("Peer list {}:{}@{}", i, peers.at(i).id, peers.at(i).address);
    }

    absl::Status status_pool = pool.Init(block_size, peers);
    ROME_ASSERT_OK(status_pool);
    ROME_INFO("Created memory pool");

    LinkedList myList = LinkedList(self, &pool);
    absl::Status status_linkedList = myList.Init(host, peers);
    ROME_ASSERT_OK(status_linkedList);

    myList.insertNode(1);
    myList.insertNode(2);
    myList.insertNode(3);
    myList.insertNode(4); 
    myList.insertNode(5);

    ROME_INFO("Contains node 1 = {}", myList.containsNode(1)); 
    ROME_INFO("Contains node 8 = {}", myList.containsNode(8)); 

    ROME_INFO("Remove node 4 = {}", myList.remove(4)); 
    ROME_INFO("Remove node 10 = {}", myList.remove(10)); 

}
