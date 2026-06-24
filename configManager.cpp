#include "ConfigManager.h"

#include <fstream>
#include <iostream>

bool ConfigManager::loadConfig(const std::string& filename)
{
    std::ifstream file(filename);

    if (!file.is_open()) {
        return false;
    }

    std::string key;

    while (file >> key) {
        if (key == "num-cpu") {
            file >> config.numCpu;
        }

        else if (key == "scheduler") {
            file >> config.scheduler;

            // remove quotes
            if (!config.scheduler.empty() && config.scheduler.front() == '"') {
                config.scheduler.erase(0, 1);
            }

            if (!config.scheduler.empty() && config.scheduler.back() == '"') {
                config.scheduler.pop_back();
            }
        }
        else if (key == "quantum-cycles") {
            file >> config.quantumCycles;
        }
        else if (key == "batch-process-freq") {
            file >> config.batchProcessFreq;
        }
        else if (key == "min-ins") {
            file >> config.minIns;
        }
        else if (key == "max-ins") {
            file >> config.maxIns;
        }
        else if (key == "delay-per-exec") {
            file >> config.delayPerExec;
        }
    }

    return true;
}

const Config& ConfigManager::getConfig() const {
    return config;
}