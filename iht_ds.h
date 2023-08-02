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
    remote_ptr<Node> head;

    LinkedList() {
        ROME_INFO("Running the linked list constructor");
    }

    // These two are going to be initialized at one point
    MemoryPool::Peer self_; //Local
    MemoryPool* pool_; //Local 

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
            remote_ptr<Node> sentinal_head = pool_->Allocate<Node>();

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
-
            // From there, try to get the head of the linked list from the node
            remote_ptr<Node> sentinal_head = decltype(sentinal_head)(host.id, got->raddr());
            this->head = sentinal_head;

        }

        return absl::OkStatus();
    }

void InitLinkedList(remote_ptr<Node> h) {
    ROME_INFO("Running the init linked list function");
    h->next = remote_nullptr; //[ojr] maybe???? 
}


void pop(){}
void push(){}


// void LinkedList::insertNode(int d) {

//     //---------------------------
//     //[ojr] Writing the next node as a null ends up in a seg fault so do this at the end ???? 
//     // pool_->Write<Node>(remote_nullptr, *(head->next)); 
//     // nodeToAdd->next = remote_nullptr; //(Replaced with the line above)
//     //---------------------------


//     //[ojr] At this point, we have allocated a head but it is not the new node 
//     if (length == 0) {

//             // Create the new node to insert
//         remote_ptr<Node> nodeToAdd = pool_->Allocate<Node>();
//         nodeToAdd->data = d;

//         ROME_INFO("Node {} getting added to head....", d);

//          pool_->Write<Node>(head, *(nodeToAdd)); 

//          // [ojr] Instead of last val pointing to null, point to a dummy value or maybe just use a length var? Nul

//         length++;         
//         ROME_INFO("Node {} has been added to head", d);
//     } 
//     else {

//          // Create the new node to insert
//         remote_ptr<Node> nodeToAdd = pool_->Allocate<Node>();
//         nodeToAdd->data = d;
        
//         ROME_INFO("Node {} getting added to end of list....", d);


// // c->next 


        
       
//         remote_ptr<Node> c = pool_->Read<Node>(head);
  
//         printf("Value at head is %d\n", c->data); 

//         //[ojr] still need to find a way how to update the pointers to iterate through list 


//             if(is_null(c->next)){ //[ojr] this code only used for 2 nodes in list ^^ (just testing)
//                 printf("next is null\n");
//                 // c->next = pool_->Allocate<Node>();
//                 pool_->Write<Node>(c->next, *(nodeToAdd));  
//             }
        

//         printf("Value at head->next is %d\n", c->next->data);
 

//     //----------------------------------------------------

//         // //[ojr] replacing line 151 with this  pool_->Write<Node>(nodeToAdd, *(c->next));   = seg fault 

//         // //[ojr] doing a pool_->Write with this also gives me a seg fault 
//         // // nodeToAdd->next = remote_nullptr;

//         // pool_->Write<Node>(remote_nullptr, *(nodeToAdd->next)); 

//         //-------------------------

//         length++; 
//         printf("LENGTH (END OF LIST) = %d\n", length); 
        
//         ROME_INFO("Node {} has been added to end of the list", d);

//     }
       
// }


// bool LinkedList::remove(int key) {
//     remote_ptr<Node> previous = remote_nullptr;
//     remote_ptr<Node> current = pool_->Read<Node>(head);
//     ROME_INFO("Past head");

//     // Iterate through the list
//     while (current != remote_nullptr) {
//         if (current->data == key) {
//             if (current == head) {
//                 head = head->next;
//                 current = head;
//                 pool_->Write<Node>(head, *current);
//                 // printList();
//             } else {
//                 previous->next = current->next;
//                 pool_->Write<Node>(current->next, *(previous->next));
//                 current = current->next;
//                 // printList();
//             }
//             return true;
//         } else {
//             previous = current;
//             pool_->Write<Node>(current, *(previous));
//             current = current->next;
//         }
//     }

//     // The key is not found in our linked list
//     return false;
// }

// bool LinkedList::containsNode(int n) {
//     remote_ptr<Node> current = pool_->Read<Node>(head);
//     // When current == null, we have gone through the whole entire list
//     while (!is_null(current)) {
//         if (current->data == n) {
//             return true;
//         }
//         current = pool_->Read<Node>(current->next);
//     }
//     // If we get to this point, we have iterated the entire list and haven't found the node
//     return false;
// }

// void LinkedList::printList() {

//     //[ojr] this code gives a remote access error even when we only have 5->2 -> null as our list 
//     //[ojr] just wrote this to test if head->next is there(line 239) gives the error 
//     remote_ptr<Node> t = pool_->Read<Node>(head); 
//               printf("%d -> ", t->data);
//     remote_ptr<Node> x = pool_->Read<Node>(head->next); 
//                   printf("%d -> ", x->data);

// //------------------------------------------------------





//     // for(int i=0; i<length; i++){
//     //       printf("%d -> ", t->data);
//     //           t = pool_->Read<Node>(t->next);
//     //   //To not seg fault -> cannot do pool_->Read with a null pointer 
//     //   if(is_null(t->next)){
//     //     printf("boom");
//     //     break;
//     //   }
//     //   else{
//     //     t = pool_->Read<Node>(t->next);
//     //   }

//     // }
//     // while (!is_null(t)) {
//     //   printf("%d -> ", t->data);
//     //   //To not seg fault -> cannot do pool_->Read with a null pointer 
//     //   if(is_null(t->next)){
//     //     break;
//     //   }
//     //   else{
//     //     t = pool_->Read<Node>(t->next);
//     //   }
//     // }
//     printf("NULL");
//     printf("\n");
// }

};