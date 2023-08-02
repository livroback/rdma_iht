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

class LinkedList {

public: 
    
    struct Node {
    int data;
    remote_ptr<Node> next;

    Node() : data(0), next(remote_nullptr) {}
    Node(int d) : data(d), next(remote_nullptr) {}
};



    typedef remote_ptr<Node> remote_node;
    remote_node head;

    LinkedList() {
        ROME_INFO("Running the linked list constructor");
    }

    // These two are going to be initialized at one point
    MemoryPool::Peer self_; //Local
    MemoryPool* pool_; //Local 

    void InitLinkedList(remote_node p);
    bool pop(); 
    void push(int d); 
    void printList(); 


    template <typename T>
    inline bool is_local(remote_ptr<T> ptr) {
        return ptr.id() == self_.id;
    }

    template <typename T>
    inline bool is_null(remote_ptr<T> ptr) {
        return ptr == remote_nullptr;
    }






    using conn_type = MemoryPool::conn_type;
    LinkedList(MemoryPool::Peer self = MemoryPool::Peer{}, MemoryPool* pool = nullptr)
        : self_(self), pool_(pool) {}

    // (Remote) Initializes the linked list by connecting to the peers and exchanging the head pointer
    absl::Status Init(MemoryPool::Peer host, const std::vector<MemoryPool::Peer>& peers) {
        bool is_host_ = self_.id == host.id;

        if (is_host_) {
            ROME_INFO("This is the host machine!");
            // Host machine, it is my responsibility to initiate configuration
            RemoteObjectProto proto;
            remote_node sentinal_head = pool_->Allocate<Node>();
            sentinal_head->data = 6; 

            // Init plist and set remote proto to communicate its value
            InitLinkedList(sentinal_head);
            //Make the head of "big" linked list to be this sentinal
            this->head = sentinal_head;

            //Next lines are going to send the pointer of the head of this big linked list to the other nodes

            proto.set_raddr(sentinal_head.address());

            // Iterate through peers
            for (auto p = peers.begin(); p != peers.end(); p++) {
                // Ignore sending pointer to myself
                if (p->id == self_.id) continue;

                // Form a connection with the machine
                auto conn_or = pool_->connection_manager()->GetConnection(p->id);
                ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

                // Send the proto over
                absl::Status status = conn_or.value()->channel()->Send(proto);
                ROME_CHECK_OK(ROME_RETURN(status), status);
            }
        } 
        else {
            // Listen for a connection
            auto conn_or = pool_->connection_manager()->GetConnection(host.id);
            ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

            // Try to get the data from the machine, repeatedly trying until successful
            auto got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
            while(got.status().code() == absl::StatusCode::kUnavailable) {
                got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
            }
            ROME_CHECK_OK(ROME_RETURN(got.status()), got);

            // From there, try to get the head of the linked list from the node
            remote_node sentinal_head = decltype(sentinal_head)(host.id, got->raddr());
            this->head = sentinal_head;
        }

        return absl::OkStatus();
    }

};

void LinkedList::InitLinkedList(remote_node h) {
    ROME_INFO("Running the init linked list function");
    h->next = remote_nullptr; //[ojr] maybe???? 
}


bool LinkedList::pop(){
    ROME_INFO("Popping from the front of list");

    //First check to make sure we have a sentinal node , if we do not return false
    if((is_null(head))){
        printf("Head is null\n");
        return false; 
    }

    //Read in the head
    remote_node sentinal = pool_->Read<Node>(head); 
    // [ojr] Error if I do not do this 
    sentinal->next = pool_->Allocate<Node>(); 


    //Move the pointer of head (head = head -> next)
    pool_->Write<Node>(sentinal, *(sentinal->next)); 

    //Deallocate where the last head was 
    pool_->Deallocate<Node>(sentinal); 

    return true; 
}



void LinkedList::push(int d){
    ROME_INFO("Pushing {} to front of list", d);
    
    //Create a node that we would like to push onto the list
    remote_node nodeToAdd = pool_->Allocate<Node>(); 
      nodeToAdd->data = d; 

    // ( [ojr] Seg fault if nodeToAdd -> next is not allocated ) 
    nodeToAdd->next = pool_->Allocate<Node>();  

  
    remote_node sentinal; 

    //Read in the sentinal node 
    if(!(is_null(head))){
        sentinal = pool_->Read<Node>(head);
        printf("The sentinal nodes value is %d\n", sentinal->data);
    }

    //nodeToAdd -> next = head 
    pool_->Write<Node>((nodeToAdd->next), *(sentinal)); 
    // (new) head = nodeToAdd
    pool_->Write<Node>((sentinal), *(nodeToAdd));
}

void LinkedList::printList() {
    remote_ptr<Node> t = pool_->Read<Node>(head); 
    while(!(is_null(t))){

          printf("%d", t->data); 
          t = t->next; 

    }

    printf("NULL");
    printf("\n");
}










