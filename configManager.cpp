#include "coreDependencies/configManager.h"
#include <fstream>
#include <iostream>
#include <sstream>

bool ConfigManager::loadConfig(const std::string& filename)
{
    std::ifstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    std::string line;
    // 1. Read line-by-line instead of word-by-word
    while (std::getline(file, line)) {
        
        // Remove leading/trailing whitespaces or skip entirely empty lines
        if (line.empty()) continue;

        // 2. Safely ignore comment lines
        // Finds if the line starts with "//". We can trim spaces if comments are indented.
        size_t firstNonSpace = line.find_first_not_of(" \t\r\n");
        if (firstNonSpace == std::string::npos || line.compare(firstNonSpace, 2, "//") == 0) {
            continue; // Skip comments and blank lines
        }

        // 3. Parse the clean line using a string stream
        std::stringstream ss(line);
        std::string key;
        ss >> key;

        if (key == "num-cpu") {
            ss >> config.numCpu;
        }
        else if (key == "scheduler") {
            ss >> config.scheduler;

            // Clean up surrounding quotes
            if (!config.scheduler.empty() && config.scheduler.front() == '"') {
                config.scheduler.erase(0, 1);
            }
            if (!config.scheduler.empty() && config.scheduler.back() == '"') {
                config.scheduler.pop_back();
            }
        }
        else if (key == "quantum-cycles") {
            ss >> config.quantumCycles;
        }
        else if (key == "batch-process-freq") {
            ss >> config.batchProcessFreq;
        }
        else if (key == "min-ins") {
            ss >> config.minIns;
        }
        else if (key == "max-ins") {
            ss >> config.maxIns;
        }
        else if (key == "delay-per-exec") {
            ss >> config.delayPerExec;
        }
        else if (key == "max-overall-mem") {
            ss >> config.maxOverallMem;
        }
        else if (key == "mem-per-frame") {
            ss >> config.memPerFrame;
        }
        else if (key == "mem-per-proc") {
            ss >> config.memPerProc;
        }
        else if(key == "varConfig"){
            ss >> config.varPrint;
        }
    }

    return true;
}

const Config& ConfigManager::getConfig() const {
    return config;
}