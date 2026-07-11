#include "coreDependencies/MemoryManager.h"

#include <fstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

MemoryManager::MemoryManager(
    uint32_t totalMemory,
    uint32_t frameSize,
    uint32_t processMemory)
    : totalMemory(totalMemory),
      frameSize(frameSize),
      processMemory(processMemory)
{
    memoryLayout.push_back({
        0,
        totalMemory,
        true,
        -1
    });
}

bool MemoryManager::allocate(int pid)
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    for (size_t i = 0; i < memoryLayout.size(); i++)
    {
        if (!memoryLayout[i].isFree)
            continue;

        if (memoryLayout[i].size < processMemory)
            continue;

        if (memoryLayout[i].size > processMemory)
        {
            MemoryBlock remaining;

            remaining.startAddress =
                memoryLayout[i].startAddress + processMemory;

            remaining.size =
                memoryLayout[i].size - processMemory;

            remaining.isFree = true;
            remaining.pid = -1;

            memoryLayout.insert(
                memoryLayout.begin() + i + 1,
                remaining
            );
        }

        // Re-access the vector after insert
        memoryLayout[i].size = processMemory;
        memoryLayout[i].isFree = false;
        memoryLayout[i].pid = pid;

        return true;
    }

    return false;
}

void MemoryManager::deallocate(int pid)
{
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

void MemoryManager::mergeFreeBlocks()
{
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

void MemoryManager::generateMemorySnapshot(int quantumCycle)
{
    std::lock_guard<std::mutex> lock(memoryMutex);

    std::ofstream out(
        "memory_stamp_" +
        std::to_string(quantumCycle) +
        ".txt");

    if (!out)
        return;

    // Timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif

    out << "Timestamp: ("
        << std::put_time(&localTime,
                         "%m/%d/%Y %I:%M:%S%p")
        << ")\n";

    //-------------------------
    // Count processes
    //-------------------------

    int processCount = 0;

    for (const auto& block : memoryLayout)
    {
        if (!block.isFree)
            processCount++;
    }

    out << "Number of processes in memory: "
        << processCount << "\n";

    //-------------------------
    // External fragmentation
    //-------------------------

    uint32_t externalFragmentation = 0;

    for (const auto& block : memoryLayout)
    {
        if (block.isFree)
            externalFragmentation += block.size;
    }

    out << "Total external fragmentation in KB: "
        << externalFragmentation
        << "\n\n";

    //-------------------------
    // Memory layout
    //-------------------------

    out << "---- end ---- = "
        << totalMemory
        << "\n\n";

    for (auto it = memoryLayout.rbegin();
         it != memoryLayout.rend();
         ++it)
    {
        const MemoryBlock& block = *it;

        out << block.startAddress + block.size
            << "\n";

        if (!block.isFree)
            out << "P" << block.pid << "\n";

        out << block.startAddress
            << "\n\n";
    }

    out << "---- start ---- = 0\n";
}