#include "CLICONTROL/ScreenSpawnerCommand.h"
#include <sstream>
#include <random>
#include <algorithm>
#include <mutex>

ScreenSpawnerCommand::ScreenSpawnerCommand() : nextPid(1) {}

bool ScreenSpawnerCommand::execute(const std::string& rawInput, const InitializeCommand& initHandler, std::mt19937& gen) {
    // Structural guard: System must be initialized to read min/max constraints
    if (!initHandler.getIsInitialized()) {
        return false;
    }

    std::stringstream ss(rawInput);
    std::string baseCmd, flag, processName;

    // Parse specific token pattern layout: "screen" "-s" "<process_name>"
    ss >> baseCmd >> flag >> processName;

    if (baseCmd != "screen" || flag != "-s" || processName.empty()) {
        return false;
    }

    // Read the instruction limits populated by the config file manager
    const Config& config = initHandler.getConfig();

    // Compute uniform random instruction budget constraints

    std::uniform_int_distribution<uint32_t> insDist(config.minIns, config.maxIns);
    int totalLines = static_cast<int>(insDist(gen));

    // Purely instantiate the Process structure and append it to our scheduler tracker
    auto newProcess = std::make_shared<Process>(nextPid++, processName, totalLines, gen, config.varPrint);
   

    {
        std::lock_guard<std::mutex> lock(listMutex);
        activeProcesses.push_back(newProcess);
    }
    return true;
}



const std::vector<std::shared_ptr<Process>>& ScreenSpawnerCommand::getActiveProcesses() const {
    std::lock_guard<std::mutex> lock(listMutex); // Now this will work
    return activeProcesses;
}

const std::vector<std::shared_ptr<Process>>& ScreenSpawnerCommand::getFinishedHistory() const {
    std::lock_guard<std::mutex> lock(listMutex); // Safe access to history too
    return finishedHistory;
}


void ScreenSpawnerCommand::cleanupFinishedProcesses() {
    // Acquire the lock for the entire duration of the cleanup operation
    std::lock_guard<std::mutex> lock(listMutex); 
    
    auto it = activeProcesses.begin();
    while (it != activeProcesses.end()) {
        if ((*it)->isFinished()) {
            finishedHistory.push_back(*it);
            it = activeProcesses.erase(it);
        } else {
            ++it;
        }
    }
}