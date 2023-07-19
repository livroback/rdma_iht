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

int length = 0;  

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
   length = 0; //Bc we dont have a head yet
}


using conn_type = MemoryPool::conn_type;
LinkedList(MemoryPool::Peer self, MemoryPool* pool) : self_(self), pool_(pool){
};


// (Remote) Initializes the linked list by connecting to the peers and exchanging the head pointer 
    absl::Status Init(MemoryPool::Peer host, const std::vector<MemoryPool::Peer> &peers) {
        bool is_host_ = self_.id == host.id;

        if (is_host_){
            ROME_INFO("This is the host machine!"); 
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
    //Create the new node to insert 
    remote_ptr<Node> nodeToAdd = pool_->Allocate<Node>(); 

    if(is_local(head)){
        printf("Head is local!");
    }
    nodeToAdd->data = d; 

    //If the head of the remote linked list is null 
    if(is_null(head)){
        //Allocate memory for the head
        head = pool_->Allocate<Node>(); 

        head=nodeToAdd; 
        head->next = remote_nullptr; 
        
        pool_->Write<Node>(nodeToAdd, *head);
        // pool_->Write<Node>(remote_nullptr, *(head->next));

        //Increment the size of the linked list
        length++;
        ROME_INFO("Head is no longer null -- node {} has been added to head", d); 
        printList(); 
    }
    
    else{
    //If the head of the linked list is no longer null, we have to find out where to put this new node
    //Start at pointer to the head and iterate to the last node in the list
    remote_ptr<Node> c = head; 
    nodeToAdd->next = remote_nullptr; 

  //  Iterate to the last node in the list and make this curr 
    while(c->next != remote_nullptr){
        c = c->next; 
    }
      c->next = nodeToAdd; 
      pool_->Write<Node>(nodeToAdd, *(c->next));

     length++; 
     ROME_INFO("Insert complete: node {} has been added to end of the list", d); 
    printList(); 
    }
}





bool remove(int key){
    remote_ptr<Node> previous = remote_nullptr; 
    remote_ptr<Node> current = pool_->Read<Node>(head);
    ROME_INFO("Past head"); 
    //Until we are done traversing the list....  
    while (current != remote_nullptr) {
      if (current->data == key) {   
          if(current == head){
            head = head->next;
            current = head;
            pool_->Write<Node>(head, *current);
            printList(); 
                  return true;
            }
        else{
           previous->next = current->next;
             pool_->Write<Node>(current->next, *(previous->next));
            current = current->next;
            printList(); 
             return true;
          }
      }
      else {
         previous = current;
         //edit 
    pool_->Write<Node>(current, *(previous));
          current = current->next;
      }
    }

    // The key is not found in our linked list 
    if (current == remote_nullptr) {
      return false;
    }


}



bool containsNode(int n){
    remote_ptr<Node> current = pool_->Read<Node>(head);
    //When current == null, we have gone through the whole entire list 
    while(current != remote_nullptr){
        if(current->data == n){
            return true; 
        }
        current = current->next; 
    }
    //If we get to this point, we have iterated the entire list and havent found the node 
    return false; 
}


void printList(){
    remote_ptr<Node> t = pool_->Read<Node>(head);
    while(t != remote_nullptr){
        printf("%d -> ", t->data);
        t = t->next; 
    }
        printf("NULL"); 
        printf("\n");

}




};
