#pragma once
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "IETThread.h"
#include "ProcessControl.h"

class RRScheduler : public IETThread
{
public:
    RRScheduler(int quantumCycles, int numCPUs, int delayPerExec);
    ~RRScheduler() override = default;

    void addProcess(std::shared_ptr<Process> process);
    void stop();

protected:
    void run() override;

private:
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::mutex queueMutex;
    std::atomic<bool> isRunning;

    int quantumLimit;
    int totalCPUs;
    int delayPerExecution;

    // Parallel vectors matching your CPU core layout
    std::vector<std::shared_ptr<Process>> cores; // Tracks which process is on which core
    std::vector<int> coreQuantumCounters;        // Tracks cycles executed *per core*
};