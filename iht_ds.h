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

class Node {
public:
    int data;
    remote_ptr<Node> next;

    Node() : data(0), next(remote_nullptr) {}
    Node(int d) : data(d), next(remote_nullptr) {}
};

class LinkedList {
public:
    remote_ptr<Node> head;

    LinkedList() {
        ROME_INFO("Running the linked list constructor");
    }

    // These two are going to be initialized at one point
    MemoryPool::Peer self_;
    MemoryPool* pool_;
    remote_ptr<LinkedList> root; // Start of the remote linked list

    void InitLinkedList(remote_ptr<LinkedList> p);

    template <typename T>
    inline bool is_local(remote_ptr<T> ptr) {
        return ptr.id() == self_.id;
    }

    template <typename T>
    inline bool is_null(remote_ptr<T> ptr) {
        return ptr == remote_nullptr;
    }

    void insertNode(int d);
    bool remove(int key);
    bool containsNode(int n);
    void printList();

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
            remote_ptr<LinkedList> ll_root = pool_->Allocate<LinkedList>();

            // Init plist and set remote proto to communicate its value
            InitLinkedList(ll_root);

            this->root = ll_root;
            proto.set_raddr(ll_root.address());

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

        } else {
            // Listen for a connection
            auto conn_or = pool_->connection_manager()->GetConnection(host.id);
            ROME_CHECK_OK(ROME_RETURN(conn_or.status()), conn_or);

            // Try to get the data from the machine, repeatedly trying until successful
            auto got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
            while (got.status().code() == absl::StatusCode::kUnavailable) {
                got = conn_or.value()->channel()->TryDeliver<RemoteObjectProto>();
            }
            ROME_CHECK_OK(ROME_RETURN(got.status()), got);

            // From there, decode the data into a value
            remote_ptr<LinkedList> ll_root = decltype(ll_root)(host.id, got->raddr());
            this->root = ll_root;
        }

        return absl::OkStatus();
    }
};

void LinkedList::InitLinkedList(remote_ptr<LinkedList> p) {
    ROME_INFO("Running the init linked list function");

    // Allocate memory for the head if it's null
    if (is_null(p->head)) {
        p->head = pool_->Allocate<Node>();
        ROME_INFO("Head is allocated");
        p->head = remote_nullptr;
    }
}

void LinkedList::insertNode(int d) {
    // Create the new node to insert
    remote_ptr<Node> nodeToAdd = pool_->Allocate<Node>();
    nodeToAdd->data = d;
    nodeToAdd->next = remote_nullptr; 
    
    if (is_null(head)) {
        ROME_INFO("Head is a null!");
        // Change local linked list
        head = pool_->Allocate<Node>();
        head = nodeToAdd; 
        ROME_INFO("Node {} has been added to head", d);
    } else {

        nodeToAdd->next = head; 
        head = nodeToAdd; 

    //     // nodeToAdd->next = head; 
    //     // head = nodeToAdd; 
    //     remote_ptr<Node> c = pool_->Read<Node>(head);

    //     // Iterate to the last node in the list
    //     while (!is_null(c->next)) {
    //               //To not seg fault -> cannot do pool_->Read with a null pointer 
    //     if(is_null(c->next)){
    //         break;
    //     }
    //         c = pool_->Read<Node>(c->next);
    //         // printf(" %d -> ", c->data);
    //     }

    //     c->next = nodeToAdd; 
    //     // printf("NULL\n ");
 }
            ROME_INFO("Node {} has been added to end of the list", d);
}


bool LinkedList::remove(int key) {
    remote_ptr<Node> previous = remote_nullptr;
    remote_ptr<Node> current = pool_->Read<Node>(head);
    ROME_INFO("Past head");

    // Iterate through the list
    while (current != remote_nullptr) {
        if (current->data == key) {
            if (current == head) {
                head = head->next;
                current = head;
                pool_->Write<Node>(head, *current);
                // printList();
            } else {
                previous->next = current->next;
                pool_->Write<Node>(current->next, *(previous->next));
                current = current->next;
                // printList();
            }
            return true;
        } else {
            previous = current;
            pool_->Write<Node>(current, *(previous));
            current = current->next;
        }
    }

    // The key is not found in our linked list
    return false;
}

bool LinkedList::containsNode(int n) {
    remote_ptr<Node> current = pool_->Read<Node>(head);
    // When current == null, we have gone through the whole entire list
    while (!is_null(current)) {
        if (current->data == n) {
            return true;
        }
        current = pool_->Read<Node>(current->next);
    }
    // If we get to this point, we have iterated the entire list and haven't found the node
    return false;
}

void LinkedList::printList() {
    remote_ptr<Node> t = pool_->Read<Node>(head); 
    while (!is_null(t)) {
      printf("%d -> ", t->data);
      //To not seg fault -> cannot do pool_->Read with a null pointer 
      if(is_null(t->next)){
        break;
      }
      else{
        t = pool_->Read<Node>(t->next);
      }
    }
    printf("NULL");
    printf("\n");
}
