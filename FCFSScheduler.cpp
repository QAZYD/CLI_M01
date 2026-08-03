#include "coreDependencies/FCFSScheduler.h"
#include "memoryControl/MemoryManager.h"
#include "coreDependencies/ProcessControl.h"
#include <iostream>
#include <chrono>

FCFSScheduler::FCFSScheduler(int cores, int delayCycles, std::shared_ptr<MemoryManager> memMgr) 
    : totalCores(cores), delayPerExec(delayCycles), memoryManager(memMgr), 
      isRunning(false), cpuCycles(0), activeWorkerCount(0),
      activeCpuTicks(0), idleCpuTicks(0) { // <-- Initialize here
    if (totalCores <= 0) totalCores = 1; 
}

void FCFSScheduler::start() {
    if (isRunning) return;
    isRunning = true;

    for (int i = 0; i < totalCores; ++i) {
        coreThreads.push_back(std::thread(&FCFSScheduler::runLoop, this, i));
    }

    masterClockThread = std::thread(&FCFSScheduler::masterClockLoop, this);
}

void FCFSScheduler::stop() {
    if (!isRunning) return;
    isRunning = false;

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

void FCFSScheduler::masterClockLoop() {
    while (isRunning) {
        {
            std::unique_lock<std::mutex> lock(tickMutex);
            
            // Wait until all worker cores finish their execution step for the current tick
            tickCv.wait(lock, [this]() { 
                return activeWorkerCount == 0 || !isRunning; 
            });

            if (!isRunning) break;

            cpuCycles++;

            // Update all sleeping processes in the waiting list
            for (auto it = waitingList.begin(); it != waitingList.end(); ) {
                (*it)->decrementSleepTicks(); 
                
                if ((*it)->getState() == Process::READY) {
                    readyQueue.push(*it); 
                    it = waitingList.erase(it); 
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

// --- MULTI-THREADED CORE LOOP WITH MEMORY INTEGRATION & EXCEPTION SAFETY ---
void FCFSScheduler::runLoop(int coreId) {
    CoreState core;
    int lastProcessedCycle = 0;

    while (isRunning) {
        std::unique_lock<std::mutex> lock(tickMutex);

        tickCv.wait(lock, [this, &lastProcessedCycle]() {
            return cpuCycles > lastProcessedCycle || !isRunning;
        });

        // Break only if stopped AND no pending cycle needs tick accounting
        if (!isRunning && cpuCycles == lastProcessedCycle) break; 
        lastProcessedCycle = cpuCycles;

        // Remember if this core was active BEFORE processing/finishing
        bool wasActiveThisTick = (core.currentProcess != nullptr);

        try {
            // 1. Core Idle Check: Grab available process from readyQueue
            if (!core.currentProcess && !readyQueue.empty()) {
                core.currentProcess = readyQueue.front();
                readyQueue.pop();
                core.currentProcess->setAssignedCore(coreId);
                core.currentProcess->setState(Process::RUNNING);

                if (core.currentProcess->getRunStartTime() == "N/A") {
                    core.currentProcess->setRunStartTime(core.currentProcess->captureCurrentTimestamp());
                }

                if (memoryManager) {
                    memoryManager->allocateProcessMemory(*core.currentProcess);
                }

                core.remainingDelayCycles = 0; 
                wasActiveThisTick = true; // Mark as active for this cycle
            }

            // 2. Core Execution Step
            if (core.currentProcess) {
                if (core.currentProcess->isFinished()) {
                    core.currentProcess->setState(Process::FINISHED);
                    if (memoryManager) {
                        memoryManager->deallocateProcessMemory(*core.currentProcess);
                    }
                    core.currentProcess->setAssignedCore(-1);
                    core.currentProcess = nullptr; 
                }
                else if (core.remainingDelayCycles > 0) {
                    core.remainingDelayCycles--;
                } 
                else {
                    if (memoryManager) {
                        core.currentProcess->executeCurrentCommand(*memoryManager);
                    } else {
                        core.currentProcess->executeCurrentCommand();
                    }

                    core.currentProcess->moveToNextLine();

                    if (core.currentProcess->getState() == Process::MEMORY_VIOLATION) {
                        if (memoryManager) {
                            memoryManager->deallocateProcessMemory(*core.currentProcess);
                        }
                        core.currentProcess->setAssignedCore(-1);
                        core.currentProcess = nullptr; 
                    } 
                    else if (core.currentProcess->isFinished()) {
                        core.currentProcess->setState(Process::FINISHED);
                        if (memoryManager) {
                            memoryManager->deallocateProcessMemory(*core.currentProcess);
                        }
                        core.currentProcess->setAssignedCore(-1);
                        core.currentProcess = nullptr; 
                    } 
                    else if (core.currentProcess->getState() == Process::WAITING) {
                        core.currentProcess->setAssignedCore(-1);
                        waitingList.push_back(core.currentProcess);
                        core.currentProcess = nullptr; 
                    } 
                    else {
                        core.remainingDelayCycles = delayPerExec; 
                    }
                }
            }
        } 
        catch (const std::exception& e) {
            std::cerr << "[Core " << coreId << " Exception]: " << e.what() << std::endl;
        } 
        catch (...) {
            std::cerr << "[Core " << coreId << " Unknown Exception]" << std::endl;
        }

        // --- Accurate Tick Recording ---
        if (wasActiveThisTick) {
            activeCpuTicks++;
        } else {
            idleCpuTicks++;
        }

        // Guarantee activeWorkerCount decrement to prevent scheduler deadlocks
        activeWorkerCount--;
        if (activeWorkerCount == 0) {
            tickCv.notify_all();
        }
    }
}

int FCFSScheduler::getCPUCycles() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(tickMutex));
    return cpuCycles;
}