#include "../coreDependencies/FCFSScheduler.h"
#include "ScreenSpawnerCommand.h"
#include <iostream>
#include <ostream> // Required for ostream
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
}