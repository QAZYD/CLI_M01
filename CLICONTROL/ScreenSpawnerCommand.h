#pragma once

#include "../coreDependencies/ProcessControl.h"
#include "InitializeCommand.h"
#include <string>
#include <vector>
#include <memory>
#include <random>
#include <mutex>

class ScreenSpawnerCommand {
private:
    mutable std::mutex listMutex;
    std::vector<std::shared_ptr<Process>> activeProcesses;
    std::vector<std::shared_ptr<Process>> finishedHistory;
    int nextPid;
    

public:
    ScreenSpawnerCommand();
    const std::vector<std::shared_ptr<Process>>& getFinishedHistory() const;
    const std::vector<std::shared_ptr<Process>>& getActiveProcesses() const;
    // Validates inputs and handles pure invocation mechanics
    bool execute(const std::string& rawInput, const InitializeCommand& initHandler, std::mt19937& gen);


    void cleanupFinishedProcesses();

    
};