#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <cstdint>

struct MemoryBlock
{
    uint32_t startAddress;
    uint32_t size;

    bool isFree;
    int pid;
};

class MemoryManager
{
public:
    MemoryManager(uint32_t totalMemory, uint32_t frameSize, uint32_t processMemory);

    // First-Fit allocation
    bool allocate(int pid);

    // Free memory occupied by a process
    void deallocate(int pid);

    // Statistics
    uint32_t getProcessesInMemory() const;
    uint32_t getExternalFragmentation() const;

    // Output
    void saveSnapshot(const std::string& filename) const;

private:
    void mergeFreeBlocks();

    uint32_t totalMemory;
    uint32_t frameSize;
    uint32_t processMemory;

    std::vector<MemoryBlock> memoryLayout;

    mutable std::mutex memoryMutex;
};