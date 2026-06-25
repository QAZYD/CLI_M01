#include "coreDependencies/RRScheduler.h"
#include <iostream>

RRScheduler::RRScheduler(int quantumCycles, int numCPUs, int delayPerExec) 
    : isRunning(true), quantumLimit(quantumCycles), totalCPUs(numCPUs), delayPerExecution(delayPerExec) 
{
    // Allocate space for our virtual processors and their quantum clocks
    cores.resize(totalCPUs, nullptr);
    coreQuantumCounters.resize(totalCPUs, 0);
}

void RRScheduler::addProcess(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    process->setState(Process::READY);
    readyQueue.push(process);
    
    std::cout << "[RR] Process " << process->getName() 
              << " (PID: " << process->getPID() << ") entered the ready queue.\n";
}

void RRScheduler::stop() {
    isRunning = false;
}

void RRScheduler::run() {
    std::cout << "[RR] Background Round Robin Scheduler Active. Cores: " << totalCPUs 
              << " | Quantum: " << quantumLimit << " cycles | Delay: " << delayPerExecution << "ms\n";

    while (isRunning) {
        bool coreActivityThisCycle = false;

        // ==========================================
        // STEP 1: LIFECYCLE MANAGEMENT (Per Core)
        // ==========================================
        for (int i = 0; i < totalCPUs; ++i) {
            
            // Scenario A: The process on this core finished its script naturally
            if (cores[i] && cores[i]->isFinished()) {
                std::cout << "[RR] [Core " << i << "] Process " << cores[i]->getName() 
                          << " [PID: " << cores[i]->getPID() << "] completed execution naturally.\n";
                cores[i] = nullptr;
            }
            
            // Scenario B: The process is still going, but its Quantum limit has run out!
            else if (cores[i] && coreQuantumCounters[i] >= quantumLimit) {
                std::cout << "[RR] [Core " << i << "] Quantum expired for Process " << cores[i]->getName() 
                          << " [PID: " << cores[i]->getPID() << "]. Preempting.\n";
                
                // Evict the process and safely put it at the back of the ready line
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    cores[i]->setState(Process::READY);
                    readyQueue.push(cores[i]);
                }
                cores[i] = nullptr; // Free up the core slot
            }

            // Scenario C: The core is currently empty (or was just emptied above). Pull new work!
            if (cores[i] == nullptr) {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!readyQueue.empty()) {
                    cores[i] = readyQueue.front();
                    readyQueue.pop();
                    
                    cores[i]->setState(Process::RUNNING);
                    coreQuantumCounters[i] = 0; // Reset the quantum usage clock for this core slot
                    
                    std::cout << "[RR] [Core " << i << "] Dispatching Process: " 
                              << cores[i]->getName() << " [PID: " << cores[i]->getPID() << "]\n";
                }
            }
        }

        // ==========================================
        // STEP 2: PARALLEL COMMAND EXECUTION
        // ==========================================
        // Now that cores are balanced, tick EVERY active core forward by exactly ONE cycle
        for (int i = 0; i < totalCPUs; ++i) {
            if (cores[i] && !cores[i]->isFinished()) {
                coreActivityThisCycle = true;
                
                std::cout << "  -> [Core " << i << "][PID " << cores[i]->getPID() << "] Executing Line " 
                          << (coreQuantumCounters[i] + 1) << "/" << quantumLimit << "\n";
                
                // Fire the script logic line
                cores[i]->executeCurrentCommand();
                cores[i]->moveToNextLine();
                
                // Advance this specific core's quantum usage record
                coreQuantumCounters[i]++;
            }
        }

        // ==========================================
        // STEP 3: SYSTEM PERFORMANCE TIMING
        // ==========================================
        if (coreActivityThisCycle) {
            if (delayPerExecution == 0) {
                IETThread::sleep(1); // Standard 1ms host fallback safety margin
            } else {
                IETThread::sleep(delayPerExecution); // Bound directly to your config string
            }
        } else {
            // No processes are currently running across any cores. Hibernate.
            IETThread::sleep(50);
        }
    }

    std::cout << "[RR] Background Round Robin Scheduler Thread Stopped.\n";
}