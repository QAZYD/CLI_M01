#include "coreDependencies/RRScheduler.h"
#include <iostream>

RRScheduler::RRScheduler(int cores, int delayCycles, int quantum) 
    : totalCores(cores), delayPerExec(delayCycles), timeQuantum(quantum), 
      isRunning(false), cpuCycles(0), activeWorkerCount(0) {
    if (totalCores <= 0) totalCores = 1; 
}

void RRScheduler::start() {
    if (isRunning) return;
    isRunning = true;

    for (int i = 0; i < totalCores; ++i) {
        coreThreads.push_back(std::thread(&RRScheduler::runLoop, this, i));
    }
    masterClockThread = std::thread(&RRScheduler::masterClockLoop, this);
}

void RRScheduler::stop() {
    if (!isRunning) return;
    isRunning = false;
    tickCv.notify_all();

    if (masterClockThread.joinable()) masterClockThread.join();
    for (auto& thread : coreThreads) {
        if (thread.joinable()) thread.join();
    }
    coreThreads.clear();
}

RRScheduler::~RRScheduler() {
    stop();
}

void RRScheduler::pushProcess(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(tickMutex);
    readyQueue.push(process);
}

void RRScheduler::masterClockLoop() {
    while (isRunning) {
        {
            std::unique_lock<std::mutex> lock(tickMutex);
            tickCv.wait(lock, [this]() { 
                return activeWorkerCount == 0 || !isRunning; 
            });

            if (!isRunning) break;
            cpuCycles++;

            // Update all sleeping processes during the clock tick phase
            for (auto it = waitingList.begin(); it != waitingList.end(); ) {
                (*it)->decrementSleepTicks(); 
                
                if ((*it)->getState() == Process::READY) {
                    readyQueue.push(*it);       // Move back to ready queue
                    it = waitingList.erase(it); // Remove from sleep tracking
                } else {
                    ++it;
                }
            }

            activeWorkerCount = totalCores;
        }
        tickCv.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void RRScheduler::runLoop(int coreId) {
    CoreState core;
    int lastProcessedCycle = 0;

    while (isRunning) {
        std::unique_lock<std::mutex> lock(tickMutex);

        tickCv.wait(lock, [this, &lastProcessedCycle]() {
            return cpuCycles > lastProcessedCycle || !isRunning;
        });

        if (!isRunning) break;
        lastProcessedCycle = cpuCycles;

        // 1. Core Idle Check: Grab a process if empty
        if (!core.currentProcess && !readyQueue.empty()) {
            core.currentProcess = readyQueue.front();
            readyQueue.pop();
            
            core.currentProcess->setAssignedCore(coreId);
            if (core.currentProcess->getRunStartTime() == "N/A") {
                core.currentProcess->setRunStartTime(core.currentProcess->captureCurrentTimestamp());
            }
            core.remainingDelayCycles = 0; 
            core.quantumUsed = 0; 
        }

        // 2. Core Execution Step
        if (core.currentProcess) {
            core.quantumUsed++;

            if (core.remainingDelayCycles > 0) {
                core.remainingDelayCycles--;
            } else {
                core.currentProcess->executeCurrentCommand();
                core.currentProcess->moveToNextLine();

                // Case A: Process finished execution entirely
                if (core.currentProcess->isFinished()) {
                    core.currentProcess = nullptr; 
                    core.quantumUsed = 0;
                } 
                // Case B: Process explicitly called SLEEP (Relinquish Core)
                else if (core.currentProcess->getState() == Process::WAITING) {
                    core.currentProcess->setAssignedCore(-1);
                    waitingList.push_back(core.currentProcess); 
                    core.currentProcess = nullptr; // Evict from core
                    core.quantumUsed = 0;          // Clear out the quantum counter
                } 
                // Case C: Round-Robin Time Quantum Expiration (Preemption)
                else if (core.quantumUsed >= timeQuantum) {
                    core.currentProcess->setAssignedCore(-1); 
                    readyQueue.push(core.currentProcess);     
                    core.currentProcess = nullptr; 
                    core.quantumUsed = 0; 
                } 
                // Case D: Retain process, enforce regular busy-wait delay execution step
                else {
                    core.remainingDelayCycles = delayPerExec; 
                }
            }
        }

        activeWorkerCount--;
        if (activeWorkerCount == 0) {
            tickCv.notify_all();
        }
    }
}

int RRScheduler::getCPUCycles() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(tickMutex));
    return cpuCycles;
}