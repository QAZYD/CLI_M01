#include "memoryControl/MemoryManager.h"
#include "coreDependencies/ProcessControl.h" // Ensures Process class layout is fully known
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>

void MemoryManager::initialize(uint32_t total_mem, uint32_t frame_size) {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    this->max_overall_mem = total_mem;
    this->mem_per_frame = frame_size;
    this->total_frames = (frame_size > 0) ? (total_mem / frame_size) : 0;

    frame_table.clear();
    for (uint32_t i = 0; i < total_frames; ++i) {
        frame_table.emplace_back(i, mem_per_frame);
    }

    pages_paged_in = 0;
    pages_paged_out = 0;
    current_tick = 0;

    // Initialize / Truncate backing store file upon startup
    std::ofstream bs(BACKING_STORE_FILE, std::ios::trunc);
    bs.close();
}

int MemoryManager::access_page(Process& proc, uint32_t page_num) {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    current_tick++;

    auto& page_table = proc.getPageTable();

    // 1. Page Hit Check
    if (page_num < page_table.size() && page_table[page_num].valid) {
        int frame_id = page_table[page_num].frame_number;
        frame_table[frame_id].last_accessed_tick = current_tick;
        return frame_id;
    }

    // 2. Page Fault Triggered!
    pages_paged_in++;
    
    int allocated_frame_id = find_free_frame();

    // 3. If no free frame exists, pick victim and evict (LRU)
    if (allocated_frame_id == -1) {
        allocated_frame_id = select_victim_frame_lru();
        evict_frame(allocated_frame_id);
    }

    // 4. Load page into allocated frame
    Frame& target_frame = frame_table[allocated_frame_id];
    target_frame.is_free = false;
    target_frame.process_name = proc.getName();
    target_frame.process_ptr = &proc;
    target_frame.virtual_page_num = page_num;
    target_frame.last_accessed_tick = current_tick;
    target_frame.dirty = false;

    // Fetch frame contents from backing store if saved previously
    read_from_backing_store(proc.getName(), page_num, target_frame.buffer);

    // Update process page table
    page_table[page_num].valid = true;
    page_table[page_num].frame_number = allocated_frame_id;

    return allocated_frame_id;
}

bool MemoryManager::write_uint16(Process& proc, uint16_t virt_addr, uint16_t value) {
    // Bounds Check: Out of Bounds Access Check
    if (virt_addr + 1 >= proc.getMemorySize()) {
        trigger_memory_violation(proc, virt_addr);
        return false;
    }

    uint32_t page_num = virt_addr / mem_per_frame;
    uint32_t offset = virt_addr % mem_per_frame;

    int frame_id = access_page(proc, page_num);

    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    Frame& frame = frame_table[frame_id];

    // Store 16-bit integer as 2 bytes (Little-Endian)
    frame.buffer[offset] = static_cast<uint8_t>(value & 0xFF);
    frame.buffer[offset + 1] = static_cast<uint8_t>((value >> 8) & 0xFF);

    frame.dirty = true;
    proc.getPageTable()[page_num].dirty = true;

    return true;
}

bool MemoryManager::read_uint16(Process& proc, uint16_t virt_addr, uint16_t& out_value) {
    // Bounds Check: Out of Bounds Access Check
    if (virt_addr + 1 >= proc.getMemorySize()) {
        trigger_memory_violation(proc, virt_addr);
        return false;
    }

    uint32_t page_num = virt_addr / mem_per_frame;
    uint32_t offset = virt_addr % mem_per_frame;

    int frame_id = access_page(proc, page_num);

    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    Frame& frame = frame_table[frame_id];

    // Reconstruct 16-bit uint from 2 bytes
    out_value = static_cast<uint16_t>(frame.buffer[offset]) |
                (static_cast<uint16_t>(frame.buffer[offset + 1]) << 8);

    return true;
}

uint32_t MemoryManager::getTotalMemory() const { 
    return max_overall_mem; 
}

uint32_t MemoryManager::getUsedMemory() const {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    uint32_t occupied_frames = 0;
    for (const auto& frame : frame_table) {
        if (!frame.is_free) occupied_frames++;
    }
    return occupied_frames * mem_per_frame;
}

uint32_t MemoryManager::getFreeMemory() const {
    return max_overall_mem - getUsedMemory();
}

uint64_t MemoryManager::getPagesPagedIn() const { 
    return pages_paged_in; 
}

uint64_t MemoryManager::getPagesPagedOut() const { 
    return pages_paged_out; 
}

int MemoryManager::find_free_frame() {
    for (size_t i = 0; i < frame_table.size(); ++i) {
        if (frame_table[i].is_free) return static_cast<int>(i);
    }
    return -1;
}

int MemoryManager::select_victim_frame_lru() {
    uint64_t oldest = UINT64_MAX;
    int victim_id = 0;
    for (size_t i = 0; i < frame_table.size(); ++i) {
        if (frame_table[i].last_accessed_tick < oldest) {
            oldest = frame_table[i].last_accessed_tick;
            victim_id = static_cast<int>(i);
        }
    }
    return victim_id;
}

void MemoryManager::evict_frame(int frame_id) {
    Frame& victim = frame_table[frame_id];
    pages_paged_out++;

    // Write to backing store if modified
    if (victim.dirty) {
        write_to_backing_store(victim.process_name, victim.virtual_page_num, victim.buffer);
    }

    // Invalidate victim page table entry on owner process
    if (victim.process_ptr != nullptr) {
        auto& pt = victim.process_ptr->getPageTable();
        if (victim.virtual_page_num >= 0 && static_cast<size_t>(victim.virtual_page_num) < pt.size()) {
            pt[victim.virtual_page_num].valid = false;
            pt[victim.virtual_page_num].frame_number = -1;
        }
    }

    victim.is_free = true;
    victim.process_name = "";
    victim.process_ptr = nullptr;
    victim.virtual_page_num = -1;
    victim.dirty = false;
}

void MemoryManager::write_to_backing_store(const std::string& proc_name, int page_num, const std::vector<uint8_t>& buffer) {
    std::ofstream bs(BACKING_STORE_FILE, std::ios::app);
    if (bs.is_open()) {
        bs << proc_name << "_P" << page_num << ":";
        for (uint8_t byte : buffer) {
            bs << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
        }
        bs << "\n";
    }
}

void MemoryManager::read_from_backing_store(const std::string& proc_name, int page_num, std::vector<uint8_t>& buffer) {
    std::fill(buffer.begin(), buffer.end(), 0);
    std::ifstream bs(BACKING_STORE_FILE);
    if (!bs.is_open()) return;

    std::string target_tag = proc_name + "_P" + std::to_string(page_num) + ":";
    std::string line;
    std::string last_matching_line = "";

    // Scan line-by-line to extract latest state of the page
    while (std::getline(bs, line)) {
        if (line.find(target_tag) == 0) {
            last_matching_line = line;
        }
    }

    if (!last_matching_line.empty()) {
        std::string hex_data = last_matching_line.substr(target_tag.length());
        std::stringstream ss(hex_data);
        std::string byte_str;
        size_t idx = 0;
        while (ss >> byte_str && idx < buffer.size()) {
            buffer[idx++] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
        }
    }
}

void MemoryManager::trigger_memory_violation(Process& proc, uint16_t fault_addr) {
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    tm local_tm{};

#if defined(_WIN32)
    localtime_s(&local_tm, &tt);
#else
    localtime_r(&tt, &local_tm);
#endif

    char time_str[9];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);
    
    proc.triggerMemoryViolation(fault_addr, std::string(time_str));
}