#pragma once
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "IETThread.h"
#include "ProcessControl.h"
#include "Scheduler.h" // Include the new interface definition

// Inherit from both IETThread and the Scheduler interface
class RRScheduler : public IETThread, public Scheduler
{
public:
    RRScheduler(int quantumCycles, int numCPUs, int delayPerExec);
    ~RRScheduler() override = default;

    // Interface overrides marked explicitly with 'override'
    void addProcess(std::shared_ptr<Process> process) override;
    void stop() override;
    
    // Moved to public so it can be called polymorphically via a Scheduler pointer
    void run() override;

private:
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::mutex queueMutex;
    std::atomic<bool> isRunning;

    int quantumLimit;
    int totalCPUs;
    int delayPerExecution;

    std::vector<std::shared_ptr<Process>> cores; 
    std::vector<int> coreQuantumCounters;        
};