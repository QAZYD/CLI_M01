#ifndef RR_SCHEDULER_H
#define RR_SCHEDULER_H

#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "IScheduler.h"
#include "ProcessControl.h" 

class MemoryManager; // Forward declaration

class RRScheduler : public IScheduler {
public:
    // Added memMgr parameter (defaults to nullptr for backward compatibility)
    RRScheduler(int cores, int delayCycles, int timeQuantum, std::shared_ptr<MemoryManager> memMgr = nullptr);
    ~RRScheduler();

    void start() override;
    void stop() override;
    void pushProcess(std::shared_ptr<Process> process) override;
    int getCPUCycles() const override;
    
    int getTotalCores() const override { 
        return totalCores; 
    }

private:
    struct CoreState {
        std::shared_ptr<Process> currentProcess = nullptr;
        int remainingDelayCycles = 0;
        int quantumUsed = 0; // Tracks how many cycles the current process has been running
    };

    int totalCores;
    int delayPerExec;   // Execution delay in CPU cycles
    int timeQuantum;    // Maximum cycles a process can hold the CPU before preemption
    std::shared_ptr<MemoryManager> memoryManager;

    bool isRunning;
    int cpuCycles;      // Central tick counter

    std::vector<std::thread> coreThreads;
    std::thread masterClockThread;

    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> waitingList;
    
    // Synchronization primitives for the cycle ticks
    mutable std::mutex tickMutex;
    std::condition_variable tickCv;
    int activeWorkerCount;

    void runLoop(int coreId);
    void masterClockLoop();
};

#endif