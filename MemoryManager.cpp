#include "coreDependencies/MemoryManager.h"

#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>

MemoryManager::MemoryManager(uint32_t totalMemory, uint32_t frameSize, uint32_t processMemory) : totalMemory(totalMemory), frameSize(frameSize), processMemory(processMemory) {
    memoryLayout.push_back({ 
        0,              // start address
        totalMemory,    // size
        true,           // free
        -1              // no process
    });
}

// First-Fit Allocation
bool MemoryManager::allocate(int pid) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    for (size_t i = 0; i < memoryLayout.size(); i++)
    {
        MemoryBlock& block = memoryLayout[i];

        if (!block.isFree)
            continue;

        if (block.size < processMemory)
            continue;

        // Split if block is larger than needed
        if (block.size > processMemory)
        {
            MemoryBlock remaining;

            remaining.startAddress =
                block.startAddress + processMemory;

            remaining.size =
                block.size - processMemory;

            remaining.isFree = true;
            remaining.pid = -1;

            memoryLayout.insert(
                memoryLayout.begin() + i + 1,
                remaining
            );
        }

        block.size = processMemory;
        block.isFree = false;
        block.pid = pid;

        return true;
    }

    return false;
}

void MemoryManager::deallocate(int pid) {
    std::lock_guard<std::mutex> lock(memoryMutex);

    for (auto& block : memoryLayout)
    {
        if (!block.isFree &&
            block.pid == pid)
        {
            block.isFree = true;
            block.pid = -1;

            mergeFreeBlocks();
            return;
        }
    }
}

void MemoryManager::mergeFreeBlocks() {
    for (size_t i = 0; i + 1 < memoryLayout.size();)
    {
        if (memoryLayout[i].isFree &&
            memoryLayout[i + 1].isFree)
        {
            memoryLayout[i].size +=
                memoryLayout[i + 1].size;

            memoryLayout.erase(
                memoryLayout.begin() + i + 1
            );
        }
        else
        {
            i++;
        }
    }
}