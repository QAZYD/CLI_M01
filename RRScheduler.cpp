#include "coreDependencies/RRScheduler.h"
#include "memoryControl/MemoryManager.h"
#include <iostream>
#include <chrono>

RRScheduler::RRScheduler(int cores, int delayCycles, int timeQuantum, std::shared_ptr<MemoryManager> memMgr) 
    : totalCores(cores), delayPerExec(delayCycles), timeQuantum(timeQuantum), memoryManager(memMgr),
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

// --- MULTI-THREADED CORE LOOP WITH QUANTUM PREEMPTION & DEMAND PAGING ---
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

                // Register process memory space
                if (memoryManager) {
                    memoryManager->allocateProcessMemory(*core.currentProcess);
                }

                core.remainingDelayCycles = 0; 
                core.quantumUsed = 0; 
            }

            // 2. Core Execution Step
            if (core.currentProcess) {
                core.quantumUsed++;

                // Guard check for processes waking up having already finished
                if (core.currentProcess->isFinished()) {
                    core.currentProcess->setState(Process::FINISHED);
                    if (memoryManager) {
                        memoryManager->deallocateProcessMemory(*core.currentProcess);
                    }
                    core.currentProcess->setAssignedCore(-1);
                    core.currentProcess = nullptr; 
                    core.quantumUsed = 0;
                }
                else if (core.remainingDelayCycles > 0) {
                    core.remainingDelayCycles--;
                } 
                else {
                    // Execute current instruction safely passing MemoryManager context
                    if (memoryManager) {
                        core.currentProcess->executeCurrentCommand(*memoryManager);
                    } else {
                        core.currentProcess->executeCurrentCommand();
                    }

                    core.currentProcess->moveToNextLine();

                    // CASE A: Memory Access Violation
                    if (core.currentProcess->getState() == Process::MEMORY_VIOLATION) {
                        if (memoryManager) {
                            memoryManager->deallocateProcessMemory(*core.currentProcess);
                        }
                        core.currentProcess->setAssignedCore(-1);
                        core.currentProcess = nullptr; 
                        core.quantumUsed = 0;
                    } 
                    // CASE B: Process finished execution entirely
                    else if (core.currentProcess->isFinished()) {
                        core.currentProcess->setState(Process::FINISHED);
                        if (memoryManager) {
                            memoryManager->deallocateProcessMemory(*core.currentProcess);
                        }
                        core.currentProcess->setAssignedCore(-1);
                        core.currentProcess = nullptr; 
                        core.quantumUsed = 0;
                    } 
                    // CASE C: Process called SLEEP (Relinquish Core)
                    else if (core.currentProcess->getState() == Process::WAITING) {
                        core.currentProcess->setAssignedCore(-1);
                        waitingList.push_back(core.currentProcess); 
                        core.currentProcess = nullptr; 
                        core.quantumUsed = 0; 
                    } 
                    // CASE D: Time Quantum Expiration (Preemption)
                    else if (core.quantumUsed >= timeQuantum) {
                        core.currentProcess->setAssignedCore(-1); 
                        core.currentProcess->setState(Process::READY);
                        readyQueue.push(core.currentProcess);     
                        core.currentProcess = nullptr; // Evicted from core, retains RAM
                        core.quantumUsed = 0; 
                    } 
                    // CASE E: Retain process on core for next instruction step
                    else {
                        core.remainingDelayCycles = delayPerExec; 
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cerr << "[RR Core " << coreId << " Exception]: " << e.what() << std::endl;
        }
        catch (...) {
            std::cerr << "[RR Core " << coreId << " Unknown Exception]" << std::endl;
        }

        // Guarantee activeWorkerCount decrement to prevent lockup
        activeWorkerCount--;
        if (activeWorkerCount == 0) {
            tickCv.notify_all();
        }
    }
}

int RRScheduler::getCPUCycles() const {
    std::lock_guard<std::mutex> lock(tickMutex);
    return cpuCycles;
}