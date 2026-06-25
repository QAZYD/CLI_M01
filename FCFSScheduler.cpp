#include "coreDependencies/FCFSScheduler.h"
#include <iostream>

FCFSScheduler::FCFSScheduler(int numCPUs, int delayPerExec) 
    : isRunning(true), totalCPUs(numCPUs), delayPerExecution(delayPerExec) 
{
    // Initialize our virtual core slots to be completely empty
    cores.resize(totalCPUs, nullptr);
}

void FCFSScheduler::addProcess(std::shared_ptr<Process> process) {
    std::lock_guard<std::mutex> lock(queueMutex);
    process->setState(Process::READY);
    readyQueue.push(process);
    
    std::cout << "[FCFS] Process " << process->getName() 
              << " (PID: " << process->getPID() << ") entered the ready queue.\n";
}

void FCFSScheduler::stop() {
    isRunning = false;
}

void FCFSScheduler::run() {
    std::cout << "[FCFS] Background Scheduler Active. Cores: " << totalCPUs 
              << " | Cycle Delay: " << delayPerExecution << "ms\n";

    while (isRunning) {
        bool coreActivityThisCycle = false;

        // Step 1: Core Lifecycle Management
        for (int i = 0; i < totalCPUs; ++i) {
            // Clean up completed processes on this core
            if (cores[i] && cores[i]->isFinished()) {
                std::cout << "[FCFS] [Core " << i << "] Process " << cores[i]->getName() 
                          << " [PID: " << cores[i]->getPID() << "] completed execution.\n";
                cores[i] = nullptr;
            }

            // If this core is empty, try to populate it with the next process in line
            if (cores[i] == nullptr) {
                std::lock_guard<std::mutex> lock(queueMutex);
                if (!readyQueue.empty()) {
                    cores[i] = readyQueue.front();
                    readyQueue.pop();
                    cores[i]->setState(Process::RUNNING);
                    
                    std::cout << "[FCFS] [Core " << i << "] Dispatching Process: " 
                              << cores[i]->getName() << " [PID: " << cores[i]->getPID() << "]\n";
                }
            }
        }

        // Step 2: Parallel Execution Phase (1 clock tick across all active cores)
        for (int i = 0; i < totalCPUs; ++i) {
            if (cores[i] && !cores[i]->isFinished()) {
                coreActivityThisCycle = true;
                
                // Advance the script by exactly ONE instruction line on this core
                cores[i]->executeCurrentCommand();
                cores[i]->moveToNextLine();
            }
        }

        // Step 3: Configurable Cycle Sleep / Performance Control
        if (coreActivityThisCycle) {
            // If delay-per-exec is 0, we apply a tiny 1ms/10ms safety sleep 
            // so your computer's real host hardware CPU doesn't spike to 100% capacity
            if (delayPerExecution == 0) {
                IETThread::sleep(1); 
            } else {
                IETThread::sleep(delayPerExecution);
            }
        } else {
            // All cores are totally idle, rest deeply until new processes are generated
            IETThread::sleep(50);
        }
    }

    std::cout << "[FCFS] Background Scheduler Thread Stopped.\n";
}