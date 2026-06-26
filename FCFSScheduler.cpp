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

// --- THE MASTER CLOCK LOOP (Your Pseudocode) ---
void FCFSScheduler::masterClockLoop() {
    while (isRunning) {
        {
            std::unique_lock<std::mutex> lock(tickMutex);
            
            // Wait until all core threads have finished processing the current cycle
            tickCv.wait(lock, [this]() { 
                return activeWorkerCount == 0 || !isRunning; 
            });

            if (!isRunning) break;

            // Increment CPU cycles exactly like your pseudocode
            cpuCycles++;

            // Reset worker status flags for the new cycle
            activeWorkerCount = totalCores;
        }

        // Broadcast to all multi-threaded cores that a new cycle has arrived
        tickCv.notify_all();
        
        // Optional: Throttle the simulation speed slightly so it doesn't max out your host system
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
                // Scheme: "Busy-waiting wherein the process remains in the CPU"
                core.remainingDelayCycles--;
            } else {
                // Scheme: "If zero, each instruction is executed per CPU cycle"
                core.currentProcess->executeCurrentCommand();
                core.currentProcess->moveToNextLine();

                if (core.currentProcess->isFinished()) {
                    core.currentProcess = nullptr; // Process completed, clear core
                } else {
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