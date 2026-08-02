#include "../coreDependencies/FCFSScheduler.h"
#include "ScreenSpawnerCommand.h"
#include "../memoryControl/memoryManager.h"
#include <iostream>
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
            const auto& historyList = spawner.getFinishedHistory();
            int totalCores = scheduler.getTotalCores();

            std::set<std::string> printedIds;

            int runningCount = 0;
            for (const auto& proc : processList) {
                if (proc->getAssignedCore() != -1 && !proc->isFinished()) {
                    runningCount++;
                }
            }

            out << "-----------------------------------------\n";
            out << " PROCESS-SMI V01.00 \n";
            out << "-----------------------------------------\n";
            out << "  CPU-Util       : " << (float)runningCount / (float)totalCores * 100.0f << "%\n";
            out << "  Memory Usage   : " << MemoryManager.getUsedMemory() << " / " << MemoryManager.getTotalMemory() << "\n";
            out << "  Memory Util    : " << static_cast<double>(MemoryManager.getUsedMemory()) / MemoryManager.getTotalMemory() * 100 << "%\n\n";
            out << "=========================================\n";
            out << " Running processes and memory usage: \n";
            out << "-----------------------------------------\n";
            out << "TODO something spawner spawner, im gonna assume that processes has memory inside of them now\n";
            out << "-----------------------------------------\n";
    }

    void printVMStat(const IScheduler& scheduler, const ScreenSpawnerCommand& spawner, const MemoryManager& memManager, std::ostream& out = std::cout) {

        out << "-----------------------------------------\n";
        out << "  Total memory     : " << MemoryManager.getTotalMemory() << "\n";
        out << "  Used memory      : " << MemoryManager.getUsedMemory() << "\n";
        out << "  Free memory      : " << MemoryManager.getFreeMemory() << "\n";
        out << "  Idle CPU ticks   : " << "TODO "<< "\n";
        out << "  Active CPU ticks : " << "TODO " << "\n";
        out << "  Total CPU ticks  : " << "TODO " << "\n";
        out << "  Num paged in     : " << MemoryManager.getPagesPagedIn() << "\n";
        out << "  Num paged out    : " << MemoryManager.getPagesPagedOut() << "\n";
        out << "-----------------------------------------\n";

    }
}