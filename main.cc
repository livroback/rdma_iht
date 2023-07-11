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

struct Node{
    int data; 
    Node* next; 
};


//methods: contains, insert, remove 

void initList(Node *head, int d){
    head->data = d; 
    head-> next = NULL; 
}


void insertNode(Node *head, int d){
    //Create the new node to insert 
    Node *nodeToAdd = new Node; 
    nodeToAdd->data = d; 
    nodeToAdd->next = NULL; 

    //until we have a place to put the node , keep on iterating 
    Node *current = head;
    //When current.next == null, the next is going to be empty spot that we can put the new node in 
    while(current->next != NULL){
        current = current->next; 
    } 

    current->next = nodeToAdd; 
}


void removeEndOfList(Node *head){
    Node *current = head; 
    Node *previous = NULL; 

    //When current.next == null, current is on the node we want to remove
    while(current->next != NULL){
        Node *next = current->next; 
        previous = current; 
        current = next; 
    }

    //Point this previous 
    previous->next = NULL; 
   
}


bool containsNode(Node *head, int n){
    Node *current = head; 
    //When current == null, we have gone through the whole entire list 
    while(current != NULL){
        if(current->data == n){
            return true; 
        }
        current = current->next; 
    }
    //If we get to this point, we have iterated the entire list and havent found the node 
    return false; 
}


void printList(Node *head){
    Node *current = head; 
    //When current == null, we have gone through the whole entire list 

    while(current != NULL){
        printf("%d -> ", current->data); 
        // ROME_INFO("{} -> ", current->data);
        current = current->next; 
    }

        printf("null");
        printf("\n"); 

}





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


	struct Node *head = new Node;
    initList(head, 6); 
    ROME_INFO("Head data is {}", head->data); 
    insertNode(head, 1);
    insertNode(head, 2); 
    insertNode(head, 3); 
    printList(head); 
    ROME_INFO("Contains node 1 = {}", containsNode(head, 1)); 
    ROME_INFO("Contains node 8 = {}", containsNode(head, 8)); 
    removeEndOfList(head); 
    printList(head); 

    // LinkedList myList = LinkedList(self, &pool); 








    // IHT iht = IHT(self, &pool);
    // absl::Status status_iht = iht.Init(host, peers);
    // ROME_ASSERT_OK(status_iht);


    //----------------------------------------------------------------------

//     // If we are node0
//     if (hostname[4] == '0')
//     {

//         IHT_Res insertResult = iht.insert(5, 10, 123456);
//         ROME_INFO("Node0 Insert Status = {}", insertResult.status);

//         // IHT_Res containsResult = iht.contains(5);
//         // IHT_Res containsResult2 = iht.contains(2);
//         // ROME_INFO("Contains 5 = {}", containsResult.status);
//         // ROME_INFO("Contains 2 = {}", containsResult2.status);
//     }
//     // If we are node1
//     else
//     {

//         // IHT_Res insertResult2 = iht.insert(2, 1, 88);
//         // ROME_INFO("Node1 Insert Status = {}", insertResult2.status);


//         IHT_Res removeResult = iht.remove(5); 
//         ROME_INFO("Node1 removing node result = {}", removeResult.status);

//         // IHT_Res containsResult = iht.contains(5);
//         // IHT_Res containsResult2 = iht.contains(2);
//         // ROME_INFO("Contains 5 = {}", containsResult.status);
//         // ROME_INFO("Contains 2 = {}", containsResult2.status);

//         // IHT_Res changeDummyValue = iht.changeDummyValue(5, 70);
//         // ROME_INFO("Node1 Change Dummy Value Status = {}", changeDummyValue.status);
//         // IHT_Res afterDummy = iht.returnDummyValue(5);
//         // ROME_INFO("Node1 dummy value = {}", afterDummy.result);
//    }



// --------------------------------------------------------------


    // IHT_Res insertResult = iht.insert(5, 10, 123456);
    // IHT_Res beforeDummy = iht.returnDummyValue(5);

    // int newVal = 100; 
    // if(insertResult.status){
    //     newVal = 0; 
    // }

    // IHT_Res changeDummyValue = iht.changeDummyValue(5, 70+newVal);
    // IHT_Res afterDummy = iht.returnDummyValue(5);

    // IHT_Res containsResult = iht.contains(5);
    // IHT_Res containsResult2 = iht.contains(123);

    //  ROME_INFO("Insert Status = {}", insertResult.status);
    //  ROME_INFO("Old dummy value = {}", beforeDummy.result);
    // ROME_INFO("Change Dummy Value Status = {}", changeDummyValue.status);
    // ROME_INFO("New dummy value = {}", afterDummy.result);

    exit(0);

    // std::vector<std::thread> threads;
    // if (hostname[4] == '0'){
    //     // If dedicated server-node, we must start the server
    //     threads.emplace_back(std::thread([&](){
    //         // We are the server
    //         std::unique_ptr<Server> server = Server::Create(host, peers, params, &pool);
    //         ROME_INFO("Server Created");
    //         absl::Status run_status = server->Launch(&done, params.runtime());
    //         ROME_ASSERT_OK(run_status);
    //         ROME_INFO("[SERVER THREAD] -- End of execution; -- ");
    //     }));
    //     if (!do_exp){
    //         done = true;
    //         // Just do server when we are running testing operations
    //         threads[0].join();
    //         exit(0);
    //     }
    // }

    // if (!do_exp){
    //     // Not doing experiment, so just create some test clients
    //     std::unique_ptr<Client> client = Client::Create(self, host, peers, params, nullptr, &iht, true);
    //     if (bulk_operations){
    //         absl::Status status = client->Operations(true);
    //         ROME_ASSERT_OK(status);
    //     } else if (test_operations) {
    //         absl::Status status = client->Operations(false);
    //         ROME_ASSERT_OK(status);
    //     }
    //     ROME_INFO("[TEST] -- End of execution; -- ");
    //     exit(0);
    // }

    // // Barrier to start all the clients at the same time
    // std::barrier client_sync = std::barrier(params.thread_count());
    // WorkloadDriverProto results[params.thread_count()];
    // for(int n = 0; n < params.thread_count(); n++){
    //     // Add the thread
    //     threads.emplace_back(std::thread([&](int index){
    //         // Create and run a client in a thread
    //         std::unique_ptr<Client> client = Client::Create(self, host, peers, params, &client_sync, &iht, index == 0);
    //         absl::StatusOr<WorkloadDriverProto> output = Client::Run(std::move(client), &done, 0.5 / (double) params.node_count());
    //         if (output.ok()){
    //             results[index] = output.value();
    //         } else {
    //             ROME_ERROR("Client run failed");
    //         }
    //         ROME_INFO("[CLIENT THREAD] -- End of execution; -- ");
    //     }, n));
    // }

    // // Join all threads
    // int i = 0;
    // for (auto it = threads.begin(); it != threads.end(); it++){
    //     ROME_INFO("Syncing {}", ++i);
    //     auto t = it;
    //     t->join();
    // }

    // *result_proto.mutable_params() = params;

    // for (int i = 0; i < params.thread_count(); i++){
    //     IHTWorkloadDriverProto* r = result_proto.add_driver();
    //     std::string output;
    //     results[i].SerializeToString(&output);
    //     r->MergeFromString(output);
    // }

    // ROME_INFO("Compiled Proto Results ### {}", result_proto.DebugString());

    // std::ofstream filestream("iht_result.pbtxt");
    // filestream << result_proto.DebugString();
    // filestream.flush();
    // filestream.close();

    // ROME_INFO("[EXPERIMENT] -- End of execution; -- ");
    // return 0;
}
