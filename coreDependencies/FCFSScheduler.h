#ifndef FCFS_SCHEDULER_H
#define FCFS_SCHEDULER_H

#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "IScheduler.h"
#include "ProcessControl.h" // Assuming this defines your Process class

class FCFSScheduler : public IScheduler{
public:
    FCFSScheduler(int cores, int delayCycles);
    ~FCFSScheduler();

    void start() override;
    void stop() override;
    void pushProcess(std::shared_ptr<Process> process) override;
    int getCPUCycles() const override;
    
    int getTotalCores() const override{ 
        return totalCores; 
    }

private:
    struct CoreState {
        std::shared_ptr<Process> currentProcess = nullptr;
        int remainingDelayCycles = 0;
        bool processingDoneForCurrentCycle = false;
    };

    int totalCores;
    int delayPerExec; // Execution delay in CPU cycles
    bool isRunning;
    int cpuCycles;    // Central tick counter

    std::vector<std::thread> coreThreads;
    std::thread masterClockThread;

    std::queue<std::shared_ptr<Process>> readyQueue;
    std::vector<std::shared_ptr<Process>> waitingList;
    // Synchronization primitives for the cycle ticks
    std::mutex tickMutex;
    std::condition_variable tickCv;
    int activeWorkerCount;

    void runLoop(int coreId);
    void masterClockLoop();
};

#endif