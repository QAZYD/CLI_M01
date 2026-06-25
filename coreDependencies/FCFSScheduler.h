#pragma once
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "IETThread.h"
#include "ProcessControl.h"

class FCFSScheduler : public IETThread
{
public:
    // Constructor now accepts configuration details directly
    FCFSScheduler(int numCPUs, int delayPerExec);
    ~FCFSScheduler() override = default;

    void addProcess(std::shared_ptr<Process> process);
    void stop();

protected:
    void run() override;

private:
    std::queue<std::shared_ptr<Process>> readyQueue;
    std::mutex queueMutex;
    std::atomic<bool> isRunning;

    // Config parameters from your pseudo-config file
    int totalCPUs;
    int delayPerExecution;

    // Array tracking what is currently running on each core
    std::vector<std::shared_ptr<Process>> cores;
};