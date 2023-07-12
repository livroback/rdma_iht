#pragma once

#include <infiniband/verbs.h>
#include <cstdint>
#include <atomic>
#include "rome/rdma/channel/sync_accessor.h"
#include "rome/rdma/connection_manager/connection.h"
#include "rome/rdma/connection_manager/connection_manager.h"
#include "rome/rdma/memory_pool/memory_pool.h"
#include "rome/rdma/rdma_memory.h"
#include "rome/logging/logging.h"
#include "common.h"

using ::rome::rdma::ConnectionManager;
using ::rome::rdma::MemoryPool;
using ::rome::rdma::remote_nullptr;
using ::rome::rdma::remote_ptr;
using ::rome::rdma::RemoteObjectProto;



class Node{ 
public: 
    int data; 
    Node* next; 

    Node(){
        this->data = 0; 
        this->next = NULL; 
    }

    Node(int data){
        this->data = data; 
        this->next = NULL; 
    }

};


class LinkedList{
public: 

    Node* head; 

    LinkedList(){
        this->head = NULL; 
    } 
    //Functions for a linked list 
    void insertNode(int d); 
    void removeEndOfList(); 
    bool containsNode(int d); 
    void printList(); 

};


void LinkedList::insertNode(int d){
    //Create the new node to insert 
    Node *nodeToAdd = new Node(d); 

    if(head == NULL){
        head = nodeToAdd; 
        return; 
    }

    //until we have a place to put the node , keep on iterating 
    Node *current = head;
    //When current.next == null, the next is going to be empty spot that we can put the new node in 
    while(current->next != NULL){
        current = current->next; 
    } 

    current->next = nodeToAdd; 
}




void LinkedList::removeEndOfList(){
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


bool LinkedList::containsNode(int n){
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


void LinkedList::printList(){
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








// MemoryPool::Peer self_;



// typedef remote_ptr<Node> remote_node; 
// remote_node head; 

// //Determines if we are dealing with a local node of the linked list? 
// bool is_local(remote_ptr<Node> ptr){
//     return ptr.id() == self_.id;
// }


// public:
//     MemoryPool* pool_;

//     using conn_type = MemoryPool::conn_type;

//     Node(MemoryPool::Peer self, MemoryPool* pool) : self_(self), pool_(pool){};

//     /// @brief Initialize the IHT by connecting to the peers and exchanging the PList pointer
//     /// @param host the leader of the initialization
//     /// @param peers all the nodes in the neighborhood
//     /// @return status code for the function
//     absl::Status Init(MemoryPool::Peer host, const std::vector<MemoryPool::Peer> &peers) {
//         bool is_host_ = self_.id == host.id;

//         if (is_host_){
//             // Host machine, it is my responsibility to initiate configuration
//             RemoteObjectProto proto;
//             remote_plist head = pool_->Allocate<Node>();


//             // Init plist and set remote proto to communicate its value
//             InitPList(iht_root, 1);






//             this->root = iht_root;
//             proto.set_raddr(iht_root.address());

//             // Iterate through peers
//             for (auto p = peers.begin(); p != peers.end(); p++){
//                 // Ignore sending pointer to myself
//                 if (p->id == self_.id) continue;

//                 // Form a connection with the machine
//                 auto conn_or = pool_->connection_manager()->GetConnection(p->id);
//                 ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

//                 // Send the proto over
//                 absl::Status status = conn_or.value()->channel()->Send(proto);
//                 ROME_CHECK_OK(ROME_RETURN(status), status);
//             }
//         } else {
//             // Listen for a connection
//             auto conn_or = pool_->connection_manager()->GetConnection(host.id);
//             ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

//             // Try to get the data from the machine, repeatedly trying until successful
//             auto got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
//             while(got.status().code() == absl::StatusCode::kUnavailable) {
//                 got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
//             }
//             ROME_CHECK_OK(ROME_RETURN(got.status()), got);

//             // From there, decode the data into a value
//             remote_plist iht_root = decltype(iht_root)(host.id, got->raddr());
//             this->root = iht_root;
//         }

//         return absl::OkStatus();
//     }











