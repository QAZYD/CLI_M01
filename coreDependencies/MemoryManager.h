#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

class MemoryManager
{
public:
    struct MemoryBlock
    {
        uint32_t startAddress;
        uint32_t size;
        bool isFree;
        int pid;
    };

    MemoryManager(
        uint32_t totalMemory,
        uint32_t frameSize,
        uint32_t processMemory);

    bool allocate(int pid);
    void deallocate(int pid);

    void generateMemorySnapshot(int quantumCycle);

private:
    void mergeFreeBlocks();

    uint32_t totalMemory;
    uint32_t frameSize;
    uint32_t processMemory;

    std::vector<MemoryBlock> memoryLayout;
    std::mutex memoryMutex;
};