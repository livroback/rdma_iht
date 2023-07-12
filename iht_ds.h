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
LinkedList() {
    ROME_INFO("Running the linked list constructor"); 
} 


//These two are going to be initialized at one point     
MemoryPool::Peer self_;
MemoryPool* pool_; 


remote_ptr<LinkedList> remote_head; //start of the remote linked list 

//At the head of the remote linked list, allocate enough space for a node (which is an int)
void InitLinkedList(remote_ptr<LinkedList> p){
    ROME_INFO("Running the init linked list function");

    Node* dummy = new Node(70); 
    //  [ojr] what do i do here??? The next two lines result in a seg fault 
    head = dummy; //Set the remote head == null????

    // *p->head = NULL; //Set the local head == null?????  
}


using conn_type = MemoryPool::conn_type;
LinkedList(MemoryPool::Peer self, MemoryPool* pool) : self_(self), pool_(pool){
};


// (Remote) Initializes the linked list by connecting to the peers and exchanging the head pointer 
    absl::Status Init(MemoryPool::Peer host, const std::vector<MemoryPool::Peer> &peers) {
        bool is_host_ = self_.id == host.id;

        if (is_host_){
            // Host machine, it is my responsibility to initiate configuration
            RemoteObjectProto proto;
            remote_ptr<LinkedList> linkedList_head = pool_->Allocate<LinkedList>();


            // Init plist and set remote proto to communicate its value
            InitLinkedList(linkedList_head);

            this->remote_head = linkedList_head;
            proto.set_raddr(linkedList_head.address());

            // Iterate through peers
            for (auto p = peers.begin(); p != peers.end(); p++){
                // Ignore sending pointer to myself
                if (p->id == self_.id) continue;

                // Form a connection with the machine
                auto conn_or = pool_->connection_manager()->GetConnection(p->id);
                ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

                // Send the proto over
                absl::Status status = conn_or.value()->channel()->Send(proto);
                ROME_CHECK_OK(ROME_RETURN(status), status);
            }

        } else {
            // Listen for a connection
            auto conn_or = pool_->connection_manager()->GetConnection(host.id);
            ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

            // Try to get the data from the machine, repeatedly trying until successful
            auto got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
            while(got.status().code() == absl::StatusCode::kUnavailable) {
                got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
            }
            ROME_CHECK_OK(ROME_RETURN(got.status()), got);

            // From there, decode the data into a value
            remote_ptr<LinkedList> linkedList_head = decltype(linkedList_head)(host.id, got->raddr());
            this->remote_head = linkedList_head;
        }

        return absl::OkStatus();
    }


inline bool is_local(remote_ptr<LinkedList> ptr){
    return ptr.id() == self_.id;
}


void insertNode(int d){
    //Read whatever is at the head of the linked list and make this curr 
    remote_ptr<LinkedList> curr = pool_->Read<LinkedList>(remote_head);


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





void removeEndOfList(){
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


bool containsNode(int n){
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


void printList(){
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




};
