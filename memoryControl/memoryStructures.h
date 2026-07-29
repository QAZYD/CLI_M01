#pragma once
#include <vector>
#include <string>
#include <cstdint>

// Forward declaration
class Process;

// --- PAGE TABLE ENTRY (Per-Process) ---
struct PageTableEntry {
    int frame_number = -1; // -1 indicates the page is paged out (not in RAM)[cite: 1]
    bool valid = false;    // true if page is loaded in physical memory[cite: 1]
    bool dirty = false;    // true if page content was modified while in RAM[cite: 1]
};

// --- PHYSICAL FRAME (Global) ---
struct Frame {
    int frame_id;
    bool is_free = true;             // Is frame available?[cite: 1]
    std::string process_name = "";   // Process currently owning this frame[cite: 1]
    Process* process_ptr = nullptr;  // Pointer to process object for page invalidation
    int virtual_page_num = -1;      // Virtual page number mapped to this frame[cite: 1]
    bool dirty = false;             // Needs flush to backing store upon eviction?[cite: 1]
    
    uint64_t last_accessed_tick = 0; // For LRU replacement[cite: 1]
    std::vector<uint8_t> buffer;     // Actual physical RAM bytes (size = mem_per_frame)[cite: 1]

    Frame(int id, size_t frame_size) : frame_id(id), buffer(frame_size, 0) {}
};