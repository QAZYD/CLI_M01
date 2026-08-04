#include "memoryControl/MemoryManager.h"
#include "coreDependencies/ProcessControl.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <iostream>

void MemoryManager::initialize(uint32_t total_mem, uint32_t frame_size) {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    this->max_overall_mem = total_mem;
    this->mem_per_frame = frame_size;
    this->total_frames = (frame_size > 0) ? (total_mem / frame_size) : 0;
    this->current_allocated_mem = 0;

    allocated_pids.clear(); // Track allocated processes
    frame_table.clear();
    for (uint32_t i = 0; i < total_frames; ++i) {
        frame_table.emplace_back(i, mem_per_frame);
    }

    pages_paged_in = 0;
    pages_paged_out = 0;
    current_tick = 0;

    // Truncate backing store file upon startup
    std::ofstream bs(BACKING_STORE_FILE, std::ios::out | std::ios::trunc);
    bs.close();
}

bool MemoryManager::allocateProcessMemory(Process& proc) {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);

    // Prevent double allocation if called by both Spawner and Scheduler
    if (allocated_pids.count(proc.getPID()) > 0) {
        return true; 
    }

    allocated_pids.insert(proc.getPID());

    // Ensure page table covers at least the initial process memory size
    uint32_t num_pages = (proc.getMemorySize() + mem_per_frame - 1) / mem_per_frame;
    auto& pt = proc.getPageTable();
    if (pt.size() < num_pages) {
        pt.resize(num_pages);
    }

    // Initialize page table entries as not in physical memory yet
    for (auto& pte : pt) {
        pte.valid = false;
        pte.frame_number = -1;
        pte.dirty = false;
    }

    return true;
}

void MemoryManager::deallocateProcessMemory(Process& proc) {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);

    if (allocated_pids.count(proc.getPID()) > 0) {
        if (current_allocated_mem >= proc.getMemorySize()) {
            current_allocated_mem -= proc.getMemorySize();
        } else {
            current_allocated_mem = 0;
        }
        allocated_pids.erase(proc.getPID());
    }

    auto& pt = proc.getPageTable();
    for (auto& pte : pt) {
        pte.valid = false;
        pte.frame_number = -1;
        pte.dirty = false;
    }

    for (size_t i = 0; i < frame_table.size(); ++i) {
        if (frame_table[i].process_ptr == &proc || frame_table[i].process_name == proc.getName()) {
            frame_table[i].is_free = true;
            frame_table[i].process_name = "";
            frame_table[i].process_ptr = nullptr;
            frame_table[i].virtual_page_num = -1;
            frame_table[i].dirty = false;
        }
    }
}

void MemoryManager::deallocateProcessMemory(int pid) {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);

    Process* target_proc = nullptr;
    for (size_t i = 0; i < frame_table.size(); ++i) {
        if (frame_table[i].process_ptr && frame_table[i].process_ptr->getPID() == pid) {
            target_proc = frame_table[i].process_ptr;
            break;
        }
    }

    if (target_proc) {
        deallocateProcessMemory(*target_proc);
    }
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

    // 2. Page Fault Triggered
    pages_paged_in++;

    int allocated_frame_id = find_free_frame();

    // 3. Evict frame using LRU if physical frames are full
    if (allocated_frame_id == -1) {
        allocated_frame_id = select_victim_frame_lru();
        evict_frame(allocated_frame_id); // Writes state to backing store if dirty
    }

    // 4. Load page into allocated frame
    Frame& target_frame = frame_table[allocated_frame_id];
    target_frame.is_free = false;
    target_frame.process_name = proc.getName();
    target_frame.process_ptr = &proc;
    target_frame.virtual_page_num = page_num;
    target_frame.last_accessed_tick = current_tick;
    target_frame.dirty = true; 

    read_from_backing_store(proc.getName(), page_num, target_frame.buffer);

    if (page_num < page_table.size()) {
        page_table[page_num].valid = true;
        page_table[page_num].frame_number = allocated_frame_id;
    }

    return allocated_frame_id;
}

bool MemoryManager::write_uint16(Process& proc, uint16_t virt_addr, uint16_t value) {
    // --- DYNAMIC VIRTUAL MEMORY & PAGE TABLE EXPANSION ---
    if (static_cast<size_t>(virt_addr) + 1 >= proc.getMemorySize()) {
        uint32_t new_size = static_cast<uint32_t>(virt_addr) + 2;
        
        // --- ADD THIS CEILING CHECK ---
        uint32_t max_virtual_address_space = 0xFFFF;
       if (new_size >  max_virtual_address_space) {
            trigger_memory_violation(proc, virt_addr);
            return false;
        }
        // ------------------------------

        proc.setMemorySize(new_size);

        uint32_t required_pages = (new_size + mem_per_frame - 1) / mem_per_frame;
        auto& pt = proc.getPageTable();
        if (pt.size() < required_pages) {
            size_t old_size = pt.size();
            pt.resize(required_pages);
            for (size_t i = old_size; i < pt.size(); ++i) {
                pt[i].valid = false;
                pt[i].frame_number = -1;
                pt[i].dirty = false;
            }
        }
    }

    if (mem_per_frame == 0) return false;
    // ... rest of your write_uint16 function ...

    std::lock_guard<std::recursive_mutex> lock(mem_mutex);

    uint32_t page0 = virt_addr / mem_per_frame;
    uint32_t offset0 = virt_addr % mem_per_frame;
    int frame0 = access_page(proc, page0);

    uint16_t addr1 = virt_addr + 1;
    uint32_t page1 = addr1 / mem_per_frame;
    uint32_t offset1 = addr1 % mem_per_frame;

    frame_table[frame0].buffer[offset0] = static_cast<uint8_t>(value & 0xFF);
    frame_table[frame0].dirty = true;
    proc.getPageTable()[page0].dirty = true;

    int frame1 = (page1 == page0) ? frame0 : access_page(proc, page1);
    frame_table[frame1].buffer[offset1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    frame_table[frame1].dirty = true;
    proc.getPageTable()[page1].dirty = true;

    return true;
}

bool MemoryManager::read_uint16(Process& proc, uint16_t virt_addr, uint16_t& out_value) {
    // --- DYNAMIC VIRTUAL MEMORY & PAGE TABLE EXPANSION ---
    if (static_cast<size_t>(virt_addr) + 1 >= proc.getMemorySize()) {
        uint32_t new_size = static_cast<uint32_t>(virt_addr) + 2;
        uint32_t max_virtual_address_space = 0xFFFF;
       if (new_size >  max_virtual_address_space) {
            trigger_memory_violation(proc, virt_addr);
            return false;
        }
        
        proc.setMemorySize(new_size);

        uint32_t required_pages = (new_size + mem_per_frame - 1) / mem_per_frame;
        auto& pt = proc.getPageTable();
        if (pt.size() < required_pages) {
            size_t old_size = pt.size();
            pt.resize(required_pages);
            for (size_t i = old_size; i < pt.size(); ++i) {
                pt[i].valid = false;
                pt[i].frame_number = -1;
                pt[i].dirty = false;
            }
        }
    }

    if (mem_per_frame == 0) return false;

    std::lock_guard<std::recursive_mutex> lock(mem_mutex);

    uint32_t page0 = virt_addr / mem_per_frame;
    uint32_t offset0 = virt_addr % mem_per_frame;
    int frame0 = access_page(proc, page0);

    uint16_t addr1 = virt_addr + 1;
    uint32_t page1 = addr1 / mem_per_frame;
    uint32_t offset1 = addr1 % mem_per_frame;

    uint8_t low_byte = frame_table[frame0].buffer[offset0];
    int frame1 = (page1 == page0) ? frame0 : access_page(proc, page1);
    uint8_t high_byte = frame_table[frame1].buffer[offset1];

    out_value = static_cast<uint16_t>(low_byte) | (static_cast<uint16_t>(high_byte) << 8);
    return true;
}

uint32_t MemoryManager::getTotalMemory() const { 
    return max_overall_mem; 
}

uint32_t MemoryManager::getUsedMemory() const {
    std::lock_guard<std::recursive_mutex> lock(mem_mutex);
    uint32_t used_frames = 0;
    for (const auto& frame : frame_table) {
        if (!frame.is_free) {
            used_frames++;
        }
    }
    return used_frames * mem_per_frame;
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
        if (!frame_table[i].is_free && frame_table[i].last_accessed_tick < oldest) {
            oldest = frame_table[i].last_accessed_tick;
            victim_id = static_cast<int>(i);
        }
    }
    return victim_id;
}

void MemoryManager::evict_frame(int frame_id) {
    Frame& victim = frame_table[frame_id];
    pages_paged_out++;

    if (victim.dirty) {
        write_to_backing_store(victim.process_name, victim.virtual_page_num, victim.buffer);
    }

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
        std::stringstream ss;
        ss << proc_name << "_P" << page_num << ":";
        for (uint8_t byte : buffer) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)byte << " ";
        }
        bs << ss.str() << "\n";
    }
}

void MemoryManager::read_from_backing_store(const std::string& proc_name, int page_num, std::vector<uint8_t>& buffer) {
    std::fill(buffer.begin(), buffer.end(), 0);
    std::ifstream bs(BACKING_STORE_FILE);
    if (!bs.is_open()) return;

    std::string target_tag = proc_name + "_P" + std::to_string(page_num) + ":";
    std::string line;
    std::string last_matching_line = "";

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