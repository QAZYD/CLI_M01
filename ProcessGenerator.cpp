#include "coreDependencies/ProcessGenerator.h"
#include "coreDependencies/CommandGenerator.h" // Groupmate's AST generator
#include <iostream>
#include <chrono>
#include <random>

ProcessGenerator::ProcessGenerator(std::shared_ptr<Scheduler> scheduler, const Config& config)
    : targetScheduler(scheduler), sysConfig(config), isRunning(false), nextAutoPID(5000) 
{
}

ProcessGenerator::~ProcessGenerator() {
    stop();
}

void ProcessGenerator::start() {
    if (!isRunning) {
        isRunning = true;
        workerThread = std::thread(&ProcessGenerator::generatorLoop, this);
    }
}

void ProcessGenerator::stop() {
    if (isRunning) {
        isRunning = false;
        if (workerThread.joinable()) {
            workerThread.join();
        }
    }
}

void ProcessGenerator::generatorLoop() {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Note: Ensure your Config struct has these exact variable names!
    std::uniform_int_distribution<uint32_t> sizeDist(sysConfig.minIns, sysConfig.maxIns);

    while (isRunning) {
        // Sleep based on your config's batch process frequency
        // (If your config uses a different variable name than batchProcessFreq, update it here!)
        std::this_thread::sleep_for(std::chrono::milliseconds(sysConfig.batchProcessFreq));

        if (!isRunning) break; // Check if we were stopped during the sleep

        // 1. Create a new process
        std::string autoName = "autoProc_" + std::to_string(nextAutoPID);
        auto newProcess = std::make_shared<Process>(nextAutoPID++, autoName);

        // 2. Use the group's advanced AST generator!
        uint32_t targetInstructions = sizeDist(gen);
        std::vector<std::shared_ptr<ICommand>> generatedProgram = CommandGenerator::generateProgram(targetInstructions);
        
        for (const auto& cmd : generatedProgram) {
            newProcess->addCommand(cmd);
        }

        // 3. Push it directly into the active OS Scheduler
        if (targetScheduler) {
            targetScheduler->addProcess(newProcess);
        }
    }
}