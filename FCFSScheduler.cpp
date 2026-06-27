#include "coreDependencies/FCFSScheduler.h"
#include <iostream>

FCFSScheduler::FCFSScheduler(int cores, int delayCycles) 
    : totalCores(cores), delayPerExec(delayCycles), isRunning(false), cpuCycles(0), activeWorkerCount(0) {
    if (totalCores <= 0) totalCores = 1; 
}

void FCFSScheduler::start() {
    if (isRunning) return;
    isRunning = true;

    // 1. Spin up the multi-threaded core workers
    for (int i = 0; i < totalCores; ++i) {
        coreThreads.push_back(std::thread(&FCFSScheduler::runLoop, this, i));
    }

    // 2. Spin up the master clock thread running your pseudocode loop
    masterClockThread = std::thread(&FCFSScheduler::masterClockLoop, this);
}

void FCFSScheduler::stop() {
    if (!isRunning) return;
    isRunning = false;

    // Wake up everything so threads can exit cleanly
    tickCv.notify_all();

    if (masterClockThread.joinable()) {
        masterClockThread.join();
    }

    for (auto& thread : coreThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    coreThreads.clear();
}

FCFSScheduler::~FCFSScheduler() {
    stop();
}

void FCFSScheduler::pushProcess(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(tickMutex);
    readyQueue.push(process);
}

// --- THE MASTER CLOCK LOOP  ---
void FCFSScheduler::masterClockLoop() {
    while (isRunning) {
        {
            std::unique_lock<std::mutex> lock(tickMutex);
            
            tickCv.wait(lock, [this]() { 
                return activeWorkerCount == 0 || !isRunning; 
            });

            if (!isRunning) break;

            // Increment CPU cycles exactly like your pseudocode
            cpuCycles++;

            //  THIS BLOCK: Update all sleeping processes
            for (auto it = waitingList.begin(); it != waitingList.end(); ) {
                (*it)->decrementSleepTicks(); // Reduces tick and changes state to READY if 0
                
                if ((*it)->getState() == Process::READY) {
                    readyQueue.push(*it);       // Put back into the execution pool
                    it = waitingList.erase(it); // Remove from tracking list
                } else {
                    ++it;
                }
            }

            // Reset worker status flags for the new cycle
            activeWorkerCount = totalCores;
        }

        tickCv.notify_all();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

// --- MULTI-THREADED CORE LOOP ---
void FCFSScheduler::runLoop(int coreId) {
    CoreState core;
    int lastProcessedCycle = 0;

    while (isRunning) {
        std::unique_lock<std::mutex> lock(tickMutex);

        // Wait for the master clock to advance to a new cycle
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
        }

        // 2. Core Execution Step
        if (core.currentProcess) {
        if (core.remainingDelayCycles > 0) {
            core.remainingDelayCycles--;
        } else {
            core.currentProcess->executeCurrentCommand();
            core.currentProcess->moveToNextLine();

            if (core.currentProcess->isFinished()) {
                core.currentProcess = nullptr; // Process completed
            } 
            //THIS CHECK FOR SLEEP RELINQUISHMENT
            else if (core.currentProcess->getState() == Process::WAITING) {
                // Remove the process from the core immediately, freeing it up
                core.currentProcess->setAssignedCore(-1);
                waitingList.push_back(core.currentProcess);
                core.currentProcess = nullptr; 
            } 
            else {
                core.remainingDelayCycles = delayPerExec; 
            }
        }
    }
        // Signal back to the master clock that this core thread is done for this cycle
        activeWorkerCount--;
        if (activeWorkerCount == 0) {
            tickCv.notify_all(); // Wake up master clock if this was the last thread
        }
    }
}

int FCFSScheduler::getCPUCycles() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(tickMutex));
    return cpuCycles;
}