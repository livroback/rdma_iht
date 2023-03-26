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

using ::rome::rdma::ConnectionManager;
using ::rome::rdma::MemoryPool;
using ::rome::rdma::remote_nullptr;
using ::rome::rdma::remote_ptr;
using ::rome::rdma::RemoteObjectProto;

// A structure to pass in the configuration for the IHT
struct config {
    size_t elist_size;
    size_t plist_size;
};

// TODO: Make it be a hashmap and not a hashset
// TODO: Make allocation of PList be dynamic and not static size

#define ELIST_SIZE 8
#define PLIST_SIZE 128

class RdmaIHT {
private:
    bool is_host_;
    MemoryPool::Peer self_;
    MemoryPool pool_;

    // "Poor-mans" enum to represent the state of a node. P-lists cannot be locked 
    const uint64_t E_LOCKED = 0;
    const uint64_t E_UNLOCKED = 1;
    const uint64_t P_UNLOCKED = 2;

    // "Super class" for the elist and plist structs
    struct Base {};
    typedef remote_ptr<Base> remote_baseptr;
    typedef remote_ptr<uint64_t> remote_lock;

    // ElementList stores a bunch of K/V pairs. IHT employs a "seperate chaining"-like approach.
    // Rather than storing via a linked list (with easy append), it uses a fixed size array
    struct EList : Base {
        /*struct pair_t {
            int key;
            int val;
        };*/

        size_t count = 0; // The number of live elements in the Elist
        int pairs[ELIST_SIZE]; // A list of pairs to store (stored as remote pointer to start of the contigous memory block)
        
        // Insert into elist a int
        void elist_insert(const int &key){
            pairs[count] = key;
            ++count;
        }

        EList(){
            ROME_DEBUG("Running EList Constructor!");
        }
    };

    // PointerList stores EList pointers and assorted locks
    struct PList : Base {
        // A pointer lock pair
        struct pair_t {
            remote_ptr<remote_baseptr> base; // Pointer to base, the super class of Elist or Plist
            remote_lock lock; // A lock to represent if the base is open or not
        };

        pair_t buckets[PLIST_SIZE]; // Pointer lock pairs

        PList(){
            ROME_INFO("Running PList Constructor!");
        }
    };

    typedef remote_ptr<PList> remote_plist;    
    typedef remote_ptr<EList> remote_elist;

    void InitPList(remote_plist p){
        ROME_INFO("Start: Init plist");
        for (size_t i = 0; i < PLIST_SIZE; i++){
            p->buckets[i].lock = pool_.Allocate<uint64_t>();
            *p->buckets[i].lock = E_UNLOCKED;
            remote_ptr<remote_baseptr> empty_base = pool_.Allocate<remote_baseptr>();
            *empty_base = remote_nullptr;
            p->buckets[i].base = empty_base;
        }
        ROME_INFO("End: Init plist");
    }

    // TODO: Transition to use these variables instead of DEFINE
    const size_t elist_size; // Size of all elists
    const size_t plist_size; // Size of all plists
    remote_ptr<PList> root;  // Start of plist
    std::hash<int> pre_hash; // Hash function from k -> size_t [this currently does nothing :: though included for templating this class]
    
    bool acquire(remote_lock lock){
        // Spin while trying to acquire the lock
        while (true){
            /*if (lock.id() != self_.id)
                lock = pool_.Read<uint64_t>(lock);
            uint64_t v = *std::to_address(lock);*/

            // If we can switch from unlock to lock status
            if (pool_.CompareAndSwap<uint64_t>(lock, E_UNLOCKED, E_LOCKED) == E_UNLOCKED){
                return true;
            }

            remote_lock local_lock = lock;
            if (lock.id() != self_.id) local_lock = pool_.Read<uint64_t>(lock);
            uint64_t v = *std::to_address(local_lock);

            // Permanent unlock
            if (v == P_UNLOCKED){
                return false;
            }
        }
    }

    // Hashing function to decide bucket size
    uint64_t level_hash(const int &key, size_t level){
        return level ^ pre_hash(key);
    }

    /// Rehash function
    /// @param parent The P-List whose bucket needs rehashing
    /// @param pcount The number of elements in `parent`
    /// @param pdepth The depth of `parent`
    /// @param pidx   The index in `parent` of the bucket to rehash
    remote_plist rehash(remote_plist parent, size_t pcount, size_t pdepth, size_t pidx){
        ROME_DEBUG("Rehash called");
        throw "Rehash not implemented yet";
        /* // TODO: the plist size should double in size
        remote_plist new_p = pool_.Allocate<PList>();
        InitPList(new_p);

        // hash everything from the full elist into it
        remote_elist source = static_cast<remote_elist>(parent->buckets[pidx].base);
        for (size_t i = 0; i < source->count; i++){
            uint64_t b = level_hash(source->pairs[i], pdepth + 1) % pcount;
            if (new_p->buckets[b].base == remote_nullptr){
                remote_elist e = pool_.Allocate<EList>();
                new_p->buckets[b].base = static_cast<remote_baseptr>(e);
            }
            remote_elist dest = static_cast<remote_elist>(new_p->buckets[b].base);
            dest->elist_insert(source->pairs[i]);
        }
        pool_.Deallocate(source);
        return new_p;*/
    }
public:
    using conn_type = MemoryPool::conn_type;

    RdmaIHT(MemoryPool::Peer self, std::unique_ptr<MemoryPool::cm_type> cm, struct config confs);

    absl::Status Init(MemoryPool::Peer host, const std::vector<MemoryPool::Peer> &peers);

    bool contains(int value){
        // start at root
        remote_plist curr = pool_.Read<PList>(root);;
        size_t depth = 1, count = plist_size;
        while (true) {
            uint64_t bucket = level_hash(value, depth) % count;
            remote_ptr<remote_baseptr> bucket_base = curr->buckets[bucket].base;
            remote_ptr<remote_baseptr> bucket_ptr = bucket_base.id() == self_.id ? bucket_base : pool_.Read<remote_baseptr>(bucket_base);
            if (!acquire(curr->buckets[bucket].lock)){
                ROME_INFO("Error: Unexpected Control Flow");
                // Can't lock then we are at a sub-plist
                // Might have to do another read here. For now its fine because we aren't rehashing
                curr = static_cast<remote_plist>(*std::to_address(bucket_ptr));
                depth++;
                count *= 1; // TODO: Change back to 2 when we expand PList size
                continue;
            }

            // Past this point we have recursed to an elist
            if (*std::to_address(bucket_ptr) == remote_nullptr){
                // empty elist
                pool_.AtomicSwap(curr->buckets[bucket].lock, E_UNLOCKED);
                return false;
            }

            // Get elist and linear search
            // Need to do a conditional read here because bucket_ptr might be ours. Basically we read the EList locally
            remote_elist e = static_cast<remote_elist>(bucket_ptr.id() == self_.id ? *bucket_ptr : pool_.Read<Base>(*std::to_address(bucket_ptr)));
            for (size_t i = 0; i < e->count; i++){
                // Linear search to determine if elist already contains the value
                if (e->pairs[i] == value){
                    pool_.AtomicSwap(curr->buckets[bucket].lock, E_UNLOCKED);
                    return true;
                }
            }

            // Can't find, unlock and return fasle
            pool_.AtomicSwap(curr->buckets[bucket].lock, E_UNLOCKED);
            return false;
        }
    }
    
    bool insert(int value){
        // start at root
        remote_plist curr = pool_.Read<PList>(root);
        size_t depth = 1, count = plist_size;
        while (true){
            uint64_t bucket = level_hash(value, depth) % count;
            remote_ptr<remote_baseptr> bucket_base = curr->buckets[bucket].base;
            remote_ptr<remote_baseptr> bucket_ptr = bucket_base.id() == self_.id ? bucket_base : pool_.Read<remote_baseptr>(bucket_base);
            if (!acquire(curr->buckets[bucket].lock)){
                // Can't lock then we are at a sub-plist
                // Might have to do another read here. For now its fine because we aren't rehashing
                curr = static_cast<remote_plist>(*std::to_address(bucket_ptr));
                depth++;
                count *= 1; // TODO: Change back to 2 when we expand PList size
                continue;
            }

            // Past this point we have recursed to an elist
            if (*std::to_address(bucket_ptr) == remote_nullptr){
                // empty elist
                remote_elist e = pool_.Allocate<EList>();
                e->elist_insert(value);
                remote_baseptr e_base = static_cast<remote_baseptr>(e);
                pool_.Write(bucket_base, e_base); // might need to do a conditional write. For now its fine because we never rehash
                pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, E_UNLOCKED);
                // successful insert
                return true;
            }

            // Need to do a conditional read here because bucket_ptr might be ours. Basically we read the EList locally
            remote_elist e = static_cast<remote_elist>(bucket_ptr.id() == self_.id ? *bucket_ptr : pool_.Read<Base>(*std::to_address(bucket_ptr)));
            for (size_t i = 0; i < e->count; i++){
                // Linear search to determine if elist already contains the value
                if (e->pairs[i] == value){
                    // Contains the value => unlock and return false
                    pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, E_UNLOCKED);
                    return false;
                }
            }

            // Check for enough insertion room
            if (e->count < elist_size) {
                // insert, unlock, return
                e->elist_insert(value);
                // If we are modifying the local copy, we need to write to the remote at the end...
                if (bucket_ptr.id() != self_.id) pool_.Write<Base>(*std::to_address(bucket_ptr), *e);
                pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, E_UNLOCKED);
                return true;
            }

            // Ignore this branch for now since we haven't implemented rehash
            // Need more room so rehash into plist and perma-unlock
            remote_plist p = rehash(curr, count, depth, bucket);
            pool_.Write(curr->buckets[bucket].base, static_cast<remote_baseptr>(pool_.Read<PList>(p)));
            pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, P_UNLOCKED);
        }
    }
    
    bool remove(int value){
        // start at root
        remote_plist curr = pool_.Read<PList>(root);
        size_t depth = 1, count = plist_size;
        while (true) {
            uint64_t bucket = level_hash(value, depth) % count;
            remote_ptr<remote_baseptr> bucket_base = curr->buckets[bucket].base;
            remote_ptr<remote_baseptr> bucket_ptr = bucket_base.id() == self_.id ? bucket_base : pool_.Read<remote_baseptr>(bucket_base);
            if (!acquire(curr->buckets[bucket].lock)){
                // Can't lock then we are at a sub-plist
                // Might have to do another read here. For now its fine because we aren't rehashing
                curr = static_cast<remote_plist>(*std::to_address(bucket_ptr));
                depth++;
                count *= 1; // TODO: Change back to 2 when we expand PList size
                continue;
            }

            // Past this point we have recursed to an elist
            if (*std::to_address(bucket_ptr) == remote_nullptr){
                // empty elist, can just unlock and return false
                pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, E_UNLOCKED);
                return false;
            }

            // Get elist and linear search
            // Need to do a conditional read here because bucket_ptr might be ours. Basically we read the EList locally
            remote_elist e = static_cast<remote_elist>(bucket_ptr.id() == self_.id ? *bucket_ptr : pool_.Read<Base>(*std::to_address(bucket_ptr)));
            for (size_t i = 0; i < e->count; i++){
                // Linear search to determine if elist already contains the value
                if (e->pairs[i] == value){
                    if (e->count > 1){
                        // Edge swap if not count=0|1
                        e->pairs[i] = e->pairs[e->count - 1];
                    }
                    e->count -= 1;
                    // If we are modifying the local copy, we need to write to the remote at the end...
                    if (bucket_ptr.id() != self_.id) pool_.Write<Base>(*std::to_address(bucket_ptr), *e);
                    pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, E_UNLOCKED);
                    return true;
                }
            }

            // Can't find, unlock and return false
            pool_.AtomicSwap<uint64_t>(curr->buckets[bucket].lock, E_UNLOCKED);
            return false;
        }
    }
};
