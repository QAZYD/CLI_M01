#include "CLICONTROL/InitializeCommand.h"
#include "CLICONTROL/ScreenSpawnerCommand.h"
#include "memoryControl/MemoryManager.h"
#include "coreDependencies/FCFSScheduler.h"
#include "coreDependencies/RRScheduler.h"
#include "CLICONTROL/Reporter.h"
#include "coreDependencies/banner.h"
#include <iostream>
#include <string>
#include <random>
#include <memory>
#include <sstream>
#include <thread>
#include <atomic>
#include <fstream>

namespace ProcessLogger {
    void printProcessReport(Process& process);
}

// Memory Validator Helper Function
bool isValidMemoryAllocation(int size) {
    if (size < 64 || size > 65536) return false;
    return (size & (size - 1)) == 0;
}

int main() {
    InitializeCommand initHandler("config.txt");
    ScreenSpawnerCommand spawnerHandler;
    
    std::unique_ptr<IScheduler> scheduler = nullptr;
    auto memoryManager = std::make_shared<MemoryManager>();
    
    std::random_device rd;
    std::mt19937 gen(rd());

    std::string userInput;
    bool isRunning = true;

    // --- AUTOMATED TESTING GENERATION CONTROL VARIABLES ---
    std::thread generatorThread;
    std::atomic<bool> isGeneratingBatch(false);
    int generatedProcessCount = 0;
    int batchProcessFreq = 1; // Fallback default frequency (in CPU ticks)

    showBanner();

    while (isRunning) {
        // Automatically clears processes that finished in the background and releases their memory
        spawnerHandler.cleanupFinishedProcesses(*memoryManager);
        
        std::cout << "root:\\ ";
        if (!std::getline(std::cin, userInput)) break;

        if (userInput == "initialize") {
            if (initHandler.execute()) {
                const auto& config = initHandler.getConfig();
                batchProcessFreq = config.batchProcessFreq;
                memoryManager->initialize(config.maxOverallMem, config.memPerFrame);
                
                // --- DYNAMIC SCHEDULER SELECTION ---
                if (config.scheduler == "rr") {
                    scheduler = std::make_unique<RRScheduler>(config.numCpu, config.delayPerExec, config.quantumCycles, memoryManager);
                } else {
                    // Default to FCFS
                    scheduler = std::make_unique<FCFSScheduler>(config.numCpu, config.delayPerExec, memoryManager);
                }
                
                scheduler->start();
                std::cout << "Scheduler (" << config.scheduler << ") activated.\n";
            }
        }

        // SCHEDULER-START ---
       // SCHEDULER-START ---
        else if (userInput == "scheduler-start") {
            if (!scheduler) {
                std::cout << "Error: System not initialized. Please run 'initialize' first.\n";
                continue;
            }

            if (isGeneratingBatch) {
                std::cout << "Test process generation is already active.\n";
                continue;
            }

            isGeneratingBatch = true;
            
            // Spin up a simple testing thread that matches process generation with CPU tick intervals
            generatorThread = std::thread([&] {
                int lastTriggeredCycle = 0;
                
// Fetch bounds from configuration
int minMem = initHandler.getConfig().minMemPerProc;
int maxMem = initHandler.getConfig().maxMemPerProc;

// Safe helper lambda to get base-2 exponent (e.g., 8 -> 3, 256 -> 8)
auto getExponent = [](int val) {
    int exp = 0;
    while (val > 1) {
        val >>= 1;
        exp++;
    }
    return exp;
};

int minExp = getExponent(minMem);
int maxExp = getExponent(maxMem);

// Safety guard: ensure minExp never exceeds maxExp
if (minExp > maxExp) {
    std::swap(minExp, maxExp);
}

std::uniform_int_distribution<int> memExponentDist(minExp, maxExp);

                while (isGeneratingBatch) {
                    int currentCycles = scheduler->getCPUCycles();

                    // Calculate if the target number of CPU cycles has passed
                    if (currentCycles >= lastTriggeredCycle + batchProcessFreq) {
                        // Update tracking cycle anchor
                        lastTriggeredCycle = currentCycles - (currentCycles % batchProcessFreq);
                        generatedProcessCount++;

                        // Generate valid power-of-2 memory size within [minMem, maxMem]
                        int randomMemSize = 1 << memExponentDist(gen);
                        std::string mockCommand = "screen -s dummy_p" + std::to_string(generatedProcessCount) + " " + std::to_string(randomMemSize);

                        // Safe-guard name validation tracking list
                        bool nameTaken = false;
                        const auto& processList = spawnerHandler.getActiveProcesses();
                        for (const auto& proc : processList) {
                            if (proc->getName() == "dummy_p" + std::to_string(generatedProcessCount)) {
                                nameTaken = true;
                                break;
                            }
                        }

                        // Generate via Spawner class if unique, syncing with Screen reattach architecture
                        if (!nameTaken) {
                            if (spawnerHandler.execute(mockCommand, initHandler, *memoryManager, gen)) {
                                if (!processList.empty()) {
                                    auto targetProcess = processList.back();
                                    scheduler->pushProcess(targetProcess);
                                }
                            }
                        }
                    }
                    
                    // Prevent busy-looping the host core while checking clock state
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            });

            std::cout << "Automated test generation started. (Generating 1 process every "
                      << batchProcessFreq << " CPU ticks)\n";
        }
        // SCHEDULER-STOP ---
        else if (userInput == "scheduler-stop") {
            if (!isGeneratingBatch) {
                std::cout << "Test process generation is not running.\n";
                continue;
            }

            isGeneratingBatch = false;
            if (generatorThread.joinable()) {
                generatorThread.join();
            }
            std::cout << "Automated test process generation stopped.\n";
        }

        else if (userInput == "screen -ls") {
            if (!scheduler) {
                std::cout << "Error: System not initialized. Run 'initialize' first.\n";
            } else {
                Reporter::printScreenLs(*scheduler, spawnerHandler);
            }
        }
        // Report Util
        else if (userInput == "report-util") {
            if (!scheduler) {
                std::cout << "Error: System not initialized. Run 'initialize' first.\n";
            } else {
                std::ofstream logFile("csopesy-log.txt");
                if (logFile.is_open()) {
                    Reporter::printScreenLs(*scheduler, spawnerHandler, logFile);
                    logFile.close();
                    std::cout << "Report successfully saved to csopesy-log.txt\n";
                } else {
                    std::cout << "Error: Could not open csopesy-log.txt for writing.\n";
                }
            }
        }      

        // Main menu version of Process-smi
        else if (userInput == "process-smi") {
            if (!scheduler) {
                std::cout << "Error: System not initialized.\n";
            } else {
                Reporter::printprocesssmi(*scheduler, spawnerHandler, *memoryManager);
            }
        }

        // VMSTAT
        else if (userInput == "vmstat") {
            if (!scheduler) {
                std::cout << "Error: System not initialized.\n";
            } else {
                Reporter::printVMStat(*scheduler, spawnerHandler, *memoryManager);
            }
        }

        // Combined handler for creating new screens (-s, -c) and entering existing screens (-r)
        else if (userInput.rfind("screen -s ", 0) == 0 || userInput.rfind("screen -c ", 0) == 0 || userInput.rfind("screen -r ", 0) == 0) {
            if (!scheduler) {
                std::cout << "Error: System not initialized. Please run 'initialize' first.\n";
                continue;
            }

            std::shared_ptr<Process> targetProcess = nullptr;
            bool isReattachCommand = (userInput.rfind("screen -r ", 0) == 0);

            if (isReattachCommand) {
                // --- CASE A: GO BACK TO AN EXISTING SCREEN ---
                std::stringstream ss(userInput);
                std::string baseCmd, flag, targetName;
                ss >> baseCmd >> flag >> targetName;

                if (targetName.empty()) {
                    std::cout << "Error: Invalid syntax. Usage: screen -r <process_name>\n";
                    continue;
                }

                bool found = false;
                const auto& processList = spawnerHandler.getActiveProcesses();
                
                for (const auto& proc : processList) {
                    if (proc->getName() == targetName) {
                        found = true;
                        if (!proc->isFinished()) {
                            targetProcess = proc; // Found it and it's still running!
                        } else {
                            std::cout << "Error: Process '" << targetName << "' has already finished execution.\n";
                        }
                        break;  
                    }
                }

                if (!found) {
                    std::cout << "Error: Process '" << targetName << "' does not exist.\n";
                }

                // If it doesn't exist or already finished, kick back to root shell immediately
                if (!targetProcess) continue;

            } else {
                // --- CASE B: SPAWN A BRAND NEW PROCESS AND SCREEN (-s or -c) ---
                std::stringstream ss(userInput);
                std::string baseCmd, flag, newProcessName, memSizeStr;
                ss >> baseCmd >> flag >> newProcessName >> memSizeStr;

                if (newProcessName.empty()) {
                    std::cout << "Error: Invalid syntax.\n";
                    continue;
                }

                bool nameTaken = false;
                const auto& processList = spawnerHandler.getActiveProcesses();
                for (const auto& proc : processList) {
                    if (proc->getName() == newProcessName) {
                        nameTaken = true;
                        break;
                    }
                }

                if (nameTaken) {
                    std::cout << "Error: A process named '" << newProcessName << "' is already running.\n";
                    continue;
                }

                if (spawnerHandler.execute(userInput, initHandler, *memoryManager, gen)) {
                    if (!processList.empty()) {
                        targetProcess = processList.back();
                        scheduler->pushProcess(targetProcess);
                    }
                } else {
                    continue;
                }
            }

            // --- UNIFIED SCREEN CONTEXT BLOCK ---
            if (targetProcess) {
                std::cout << "\033[2J\033[1;1H" << std::flush;
                std::cout << "Switched to process screen context: " << targetProcess->getName() << "\n";
                ProcessLogger::printProcessReport(*targetProcess);
                
                bool inProcessScreen = true;
                std::string processInput;

                while (inProcessScreen) {
                    std::cout << targetProcess->getName() << ":\\ ";
                    if (!std::getline(std::cin, processInput)) break;

                    if (processInput == "process-smi") {
                        ProcessLogger::printProcessReport(*targetProcess);
                        if (targetProcess->isFinished()) {
                            std::cout << "(Process has finished running in the background.)\n";
                        }
                    }
                    else if (processInput == "exit") {
                        std::cout << "\033[2J\033[1;1H" << std::flush;
                        inProcessScreen = false;
                        showBanner();
                    }
                }
            }
        }
        else if (userInput == "exit") {
            std::cout << "Shutting down background scheduler and exiting shell...\n";
            isGeneratingBatch = false;
            if (generatorThread.joinable()) {
                generatorThread.join();
            }
            if (scheduler) {
                scheduler->stop();
            }
            isRunning = false;
        }
    }
    return 0;
}