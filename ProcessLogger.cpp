#include "coreDependencies/ProcessControl.h"
#include <iostream>
#include <string>

namespace ProcessLogger {

    void printProcessReport(Process& process) {
        // Main header layout structure
        std::cout << "process name: <" << process.getName() << ">\n";
        std::cout << "ID: " << process.getPID() << "\n";
        
        // Added right before Logs:
        std::cout << "current instruction line: " << process.getCurrentInstructionLine() << "\n";
        std::cout << "Lines of code: " << process.getTotalLines() << "\n";
        std::cout << "Lines executed: " << process.getLinesExecuted() << "\n";
        
        std::cout << "Logs:\n";

        int core = process.getAssignedCore();
        const auto& executedInstructions = process.getCommandLogs();

        // Loop through and print the pure instruction logs
        for (const auto& logEntry : executedInstructions) {
            std::cout << " (" << logEntry.timestamp << ") "
                      << "CORE: " << (core == -1 ? "N/A" : std::to_string(core)) << " "
                      << "<" << logEntry.commandText << ">\n";
        }


    }
}