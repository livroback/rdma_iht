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
    remote_ptr<Node> next;

    Node(){
        this->data = 0;
    }
    Node(int data){
        this->data = data; 
    }
};


class LinkedList{

public: 
remote_ptr<Node> head; 
int length; 

LinkedList() {
    ROME_INFO("Running the linked list constructor"); 
} 


//These two are going to be initialized at one point     
MemoryPool::Peer self_;
MemoryPool* pool_; 
remote_ptr<LinkedList> root; //start of the remote linked list 

void InitLinkedList(remote_ptr<LinkedList> p){

    ROME_INFO("Running the init linked list function");
    //Make the remote head null 
    p->head = remote_nullptr; 
    //Make head's next null as well 
    // p->head->next = remote_nullptr; 

   p->length = 0; //Bc we dont have a head yet
   ROME_INFO("Value of length is {}", p->length); 
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
            remote_ptr<LinkedList> ll_root = pool_->Allocate<LinkedList>();


            // Init plist and set remote proto to communicate its value
            InitLinkedList(ll_root);

            this->root = ll_root;
            proto.set_raddr(ll_root.address());

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
            remote_ptr<LinkedList> ll_root = decltype(ll_root)(host.id, got->raddr());
            this->root = ll_root;
        }

        return absl::OkStatus();
    }


inline bool is_local(remote_ptr<LinkedList> ptr){
    return ptr.id() == self_.id;
}

inline bool is_null(remote_ptr<LinkedList> ptr){
    return ptr == remote_nullptr;
}

inline bool is_local(remote_ptr<Node> ptr){
    return ptr.id() == self_.id;
}

inline bool is_null(remote_ptr<Node> ptr){
    return ptr == remote_nullptr;
}





void insertNode(int d){
    
     ROME_INFO("Value of length is {} at line 141", length); 

    printf("Insert node function!\n"); 

    //Create the new node to insert 
    remote_ptr<Node> nodeToAdd = pool_->Allocate<Node>(); 
    nodeToAdd->data = d; 

    //If the head of the remote linked list is null 
    if(is_null(head)){
        ROME_INFO("Head is null in our remote linked list so we are going to make node {} the head",d); 
        //Allocate memory for the head
        head = pool_->Allocate<Node>(); 

        //Make this node the head --> head = nodeToAdd 
      //  pool_->Write<Node>(nodeToAdd, *head); 
        head=nodeToAdd; 

        // Make head.next = null
        head->next = remote_nullptr;
 
        //Increment the size of the linked list
        length++;

        if(!is_null(head)){
        ROME_INFO("Head is no longer null -- node {} has been added to head", d); 
        }


    ROME_INFO("Value of length is {}", length); 
        return; 
    }

    printf("Head of the linked list is no longer null, find out where to put this new node\n"); 


    //If the head of the linked list is no longer null, we have to find out where to put this new node

    //Start at pointer to the head and iterate to the last node in the list 
    remote_ptr<Node> curr = pool_->Read<Node>(head);
    ROME_INFO("Value at curr is {}", curr->data); 

  //  Iterate to the last node in the list and make this curr 
    for(int i=0; i<length-1; i++){
       curr = curr->next; 
           ROME_INFO("Value at new curr is {}", curr->data); 
    }



    //Curr now holds the last node, so last node.next should be this new node -->  curr->next = nodeToAdd;
    curr->next = nodeToAdd; 
 //   pool_->Write<Node>(nodeToAdd, *curr->next); 


    //Make the newNode.next point to null 
    nodeToAdd->next = remote_nullptr; 
    length++; 
     ROME_INFO("Insert complete: node {} has been added to end of the list", d); 

    ROME_INFO("Value of length is {} at line 198", length); 
    return; 

}





// void removeEndOfList(){
//     Node *current = head; 
//     Node *previous = NULL; 

//     //When current.next == null, current is on the node we want to remove
//     while(current->next != NULL){
//         Node *next = current->next; 
//         previous = current; 
//         current = next; 
//     }

//     //Point this previous 
//     previous->next = NULL; 
   
// }



// bool containsNode(int n){
//     Node *current = head; 
//     //When current == null, we have gone through the whole entire list 
//     while(current != NULL){
//         if(current->data == n){
//             return true; 
//         }
//         current = current->next; 
//     }
//     //If we get to this point, we have iterated the entire list and havent found the node 
//     return false; 
// }


void printList(){
    //Start at the head
    remote_ptr<Node> current = pool_->Read<Node>(head); 
    printf("%d -> ", current->data); 

    //When current == null, we have gone through the whole entire list 

    // for(int i=0; i<length; i++){
    //     printf("%d -> ", current->data); 
    //     current = pool_->Read<Node>(current->next);; 
    // }


    // if(current == remote_nullptr){
    //     printf("null");
    //     printf("\n"); 
    // }

}




};
