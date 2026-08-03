#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include "memoryStructures.h"

// Forward declaration to break header dependency loops
class Process;

class MemoryManager {
private:
    uint32_t max_overall_mem = 0;  // Total physical RAM in bytes
    uint32_t mem_per_frame = 0;    // Frame size in bytes
    uint32_t total_frames = 0;     // max_overall_mem / mem_per_frame
    
    std::vector<Frame> frame_table; // Global Physical RAM
    
    // Recursive mutex allows nested locking from the same thread without deadlocking
    mutable std::recursive_mutex mem_mutex; 

    // Global Statistics for "vmstat" command
    uint64_t pages_paged_in = 0;
    uint64_t pages_paged_out = 0;
    uint64_t current_tick = 0;     // System tick tracking for LRU

    const std::string BACKING_STORE_FILE = "csopesy-backing-store.txt";

public:
    MemoryManager() = default;

    void initialize(uint32_t total_mem, uint32_t frame_size);

    // --- DEMAND PAGING ENGINE ---
    int access_page(Process& proc, uint32_t page_num);

    // --- SAFE READ / WRITE OPERATIONS FOR INSTRUCTIONS ---
    bool write_uint16(Process& proc, uint16_t virt_addr, uint16_t value);
    bool read_uint16(Process& proc, uint16_t virt_addr, uint16_t& out_value);

    // --- DIAGNOSTIC STATS FOR "vmstat" AND "process-smi" ---
    uint32_t getTotalMemory() const;
    uint32_t getUsedMemory() const;
    uint32_t getFreeMemory() const;
    uint64_t getPagesPagedIn() const;
    uint64_t getPagesPagedOut() const;

    // Process lifecycle memory management methods:
    bool allocateProcessMemory(Process& proc);
    void deallocateProcessMemory(Process& proc);
    void deallocateProcessMemory(int pid);

private:
    int find_free_frame();
    int select_victim_frame_lru();
    void evict_frame(int frame_id);
    void write_to_backing_store(const std::string& proc_name, int page_num, const std::vector<uint8_t>& buffer);
    void read_from_backing_store(const std::string& proc_name, int page_num, std::vector<uint8_t>& buffer);
    void trigger_memory_violation(Process& proc, uint16_t fault_addr);
};