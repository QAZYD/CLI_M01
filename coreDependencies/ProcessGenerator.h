#pragma once
#include <memory>
#include <thread>
#include <atomic>
#include "coreDependencies/Scheduler.h"
#include "configManager.h" 

class ProcessGenerator {
public:
    ProcessGenerator(std::shared_ptr<Scheduler> scheduler, const Config& config);
    ~ProcessGenerator();

    void start();
    void stop();

private:
    void generatorLoop();

    std::shared_ptr<Scheduler> targetScheduler;
    Config sysConfig;
    
    std::thread workerThread;
    std::atomic<bool> isRunning;
    uint32_t nextAutoPID;
};