#include "CLICONTROL/InitializeCommand.h"
#include <iostream>

InitializeCommand::InitializeCommand(const std::string& filePath)
    : isInitialized(false), configFilePath(filePath) {}

bool InitializeCommand::execute() {
    if (isInitialized) {
        std::cout << "System Error: System is already initialized.\n";
        return false;
    }

    if (configManager.loadConfig(configFilePath)) {
        isInitialized = true;
        std::cout << "ConfigFile loaded successfully via 'initialize' command!\n";
        printConfigSummary();   
        return true;
    } else {
        std::cout << "Initialization Failed: Could not locate or open '" << configFilePath << "'.\n";
        return false;
    }
}

bool InitializeCommand::getIsInitialized() const {
    return isInitialized;
}

const Config& InitializeCommand::getConfig() const {
    return configManager.getConfig();
}

void InitializeCommand::printConfigSummary() const {
    const Config& loadedConfig = configManager.getConfig();
    std::cout << "-----------------------------------------\n";
    std::cout << "  CPUs Allocated       : " << loadedConfig.numCpu << "\n";
    std::cout << "  Scheduler Selected   : " << loadedConfig.scheduler << "\n";
    std::cout << "  Quantum Cycles       : " << loadedConfig.quantumCycles << "\n";
    std::cout << "  Batch Frequency      : " << loadedConfig.batchProcessFreq << "\n";
    std::cout << "  Min Instructions     : " << loadedConfig.minIns << "\n";
    std::cout << "  Max Instructions     : " << loadedConfig.maxIns << "\n";
    std::cout << "  Delay Per Execution  : " << loadedConfig.delayPerExec << "ms\n";
    std::cout << "  Will print var       : " << loadedConfig.varPrint << "\n";
    std::cout << "Max overall mem        : " << loadedConfig.maxoverallMem << "\n";
    std::cout << "Mem per frame          : " << loadedConfig.memperframe << "\n";
    std::cout << "Min mem per proc       : " << loadedConfig.minmemperproc << "\n";
    std::cout << "Max mem per proc       : " << loadedConfig.maxmemperproc << "\n";
    std::cout << "-----------------------------------------\n";
}