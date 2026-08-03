#include "../coreDependencies/FCFSScheduler.h"
#include "../coreDependencies/ProcessControl.h"
#include "ScreenSpawnerCommand.h"
#include "../memoryControl/memoryManager.h"
#include <iostream>
#include <iomanip>
#include <ostream> 
#include <set>

namespace Reporter {
    // Modified to accept an output stream (defaults to std::cout)
    void printScreenLs(const IScheduler& scheduler, const ScreenSpawnerCommand& spawner, std::ostream& out = std::cout) {
        const auto& processList = spawner.getActiveProcesses();
        const auto& historyList = spawner.getFinishedHistory();
        int totalCores = scheduler.getTotalCores();

        std::set<std::string> printedIds;

        int runningCount = 0;
        for (const auto& proc : processList) {
            if (proc->getAssignedCore() != -1 && !proc->isFinished()) {
                runningCount++;
            }
        }

        // Use 'out' instead of 'std::cout'
        out << "CPU Utilization: " << (float)runningCount / (float)totalCores * 100.0f << "%\n";
        out << "Cores Used: " << runningCount << " | Cores Available: " << (totalCores - runningCount) << "\n";
        out << "-------------------------------------------------------\n";

        auto printUnique = [&](const std::shared_ptr<Process>& proc, const std::string& state) {
            if (printedIds.find(proc->getName()) == printedIds.end()) {
                out << proc->getName() << " (" << (proc->getAssignedCore() == -1 ? "WAITING" : proc->getRunStartTime()) << ") "
                          << (proc->getAssignedCore() != -1 ? "core:" + std::to_string(proc->getAssignedCore()) + " " : "")
                          << "<" << proc->getLinesExecuted() << "/" << proc->getTotalLines() << "> " 
                          << state << "\n";
                printedIds.insert(proc->getName());
            }
        };

        out << "Running Processes:\n";
        for (const auto& proc : processList) {
            if (proc->getAssignedCore() != -1 && !proc->isFinished()) printUnique(proc, "");
        }

        out << "-------------------------------------------------------\n";
        out << "Waiting in Queue:\n";
        for (const auto& proc : processList) {
            if (proc->getAssignedCore() == -1 && !proc->isFinished()) printUnique(proc, "");
        }

        out << "-------------------------------------------------------\n";
        out << "Finished Processes:\n";
        for (const auto& proc : historyList) {
             if (printedIds.find(proc->getName()) == printedIds.end()) {
                out << proc->getName() << " (" << proc->getRunStartTime() << ") FINISHED <" 
                          << proc->getTotalLines() << "/" << proc->getTotalLines() << ">\n";
                printedIds.insert(proc->getName());
             }
        }
    }
void printprocesssmi(const IScheduler& scheduler, const ScreenSpawnerCommand& spawner, const MemoryManager& MemoryManager, std::ostream& out = std::cout) {
    const auto& processList = spawner.getActiveProcesses();
    int totalCores = scheduler.getTotalCores();

    int runningCount = 0;
    for (const auto& proc : processList) {
        if (proc && proc->getAssignedCore() != -1 && !proc->isFinished()) {
            runningCount++;
        }
    }

    float cpuUtil = totalCores > 0 ? ((float)runningCount / (float)totalCores) * 100.0f : 0.0f;
    size_t usedMem = MemoryManager.getUsedMemory();
    size_t totalMem = MemoryManager.getTotalMemory();
    double memUtil = totalMem > 0 ? (static_cast<double>(usedMem) / totalMem) * 100.0 : 0.0;

    out << "-----------------------------------------\n";
    out << " PROCESS-SMI V01.00 \n";
    out << "-----------------------------------------\n";
    out << "  CPU-Util       : " << cpuUtil << "%\n";
    out << "  Memory Usage   : " << usedMem << " / " << totalMem << " bytes\n";
    out << "  Memory Util    : " << memUtil << "%\n\n";
    out << "=========================================\n";
    out << " Running processes and memory usage: \n";
    out << "-----------------------------------------\n";

    bool foundRunning = false;
    for (const auto& proc : processList) {
        // Display non-finished active processes
        if (proc && !proc->isFinished()) {
            foundRunning = true;
            out << " " << std::left << std::setw(20) << proc->getName() 
                << proc->getMemorySize() << " bytes\n";
        }
    }

    if (!foundRunning) {
        out << " (No running processes)\n";
    }

    out << "-----------------------------------------\n";
}
   void printVMStat(const IScheduler& scheduler, const ScreenSpawnerCommand& spawner, const MemoryManager& MemoryManager, std::ostream& out = std::cout) {
    size_t totalMem = MemoryManager.getTotalMemory();
    size_t usedMem  = MemoryManager.getUsedMemory();
    size_t freeMem  = MemoryManager.getFreeMemory();

    // Pull CPU ticks from scheduler state
    int idleTicks   = scheduler.getIdleCPUTicks();   // Accumulation of idle cycles across cores
    int activeTicks = scheduler.getActiveCPUTicks(); // Accumulation of execution cycles across cores
    int totalTicks  = scheduler.getCPUCycles();      // Total clock ticks passed

    out << "-----------------------------------------\n";
    out << "  Total memory     : " << totalMem << " bytes\n";
    out << "  Used memory      : " << usedMem << " bytes\n";
    out << "  Free memory      : " << freeMem << " bytes\n";
    out << "  Idle CPU ticks   : " << idleTicks << "\n";
    out << "  Active CPU ticks : " << activeTicks << "\n";
    out << "  Total CPU ticks  : " << totalTicks << "\n";
    out << "  Num paged in     : " << MemoryManager.getPagesPagedIn() << "\n";
    out << "  Num paged out    : " << MemoryManager.getPagesPagedOut() << "\n";
    out << "-----------------------------------------\n";
}
}