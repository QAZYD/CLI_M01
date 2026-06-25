#include "coreDependencies/CommandProcessor.h"
#include "coreDependencies/FCFSScheduler.h"
#include "coreDependencies/RRScheduler.h"
#include <iostream>
#include <algorithm>
#include <iomanip>
#include <random>
#include <fstream>

CommandProcessor::CommandProcessor() {
    commandMap["initialize"]      = &CommandProcessor::handleInitialize;
    commandMap["screen_ls"]       = &CommandProcessor::handleScreen_ls;
    commandMap["screen-ls"]       = &CommandProcessor::handleScreen_ls;
    commandMap["screen_r"]        = &CommandProcessor::handleScreen_r;
    commandMap["scheduler-start"] = &CommandProcessor::handleSchedulerStart;
    commandMap["scheduler-stop"]  = &CommandProcessor::handleSchedulerStop;
    commandMap["report-util"]     = &CommandProcessor::handleReportUtil;
    commandMap["?"]               = &CommandProcessor::handleHelp;
}

std::string CommandProcessor::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

void CommandProcessor::clearScreen() {
    // Standard ANSI escape code to clear terminal and reset cursor position
    std::cout << "\033[2J\033[1;1H";
}

bool CommandProcessor::execute(const std::string& input) {
    std::string trimmed = trim(input);
    if (trimmed.empty()) return true;

    // =========================================================================
    // CONTEXT A: INSIDE A PROCESS SCREEN
    // =========================================================================
    if (currentAttachedProcess != nullptr) {
        
        if (trimmed == "process-smi") {
            handleProcessSMI();
        } 
        else if (trimmed == "exit") {
            currentAttachedProcess = nullptr; // Detach context
            clearScreen();
            std::cout << "  Returned to Main Menu Console.\n";
        } 
        else {
            std::cout << "  Invalid Command inside Process Screen. Options: 'process-smi', 'exit'\n";
        }
        return true;
    }

    // =========================================================================
    // CONTEXT B: MAIN MENU CONSOLE
    // =========================================================================
    if (trimmed == "exit") {
        std::cout << "\n  exiting...\n";
        return false;
    }

    // Dynamic Parsing Intercept: Check if command matches 'screen -s <name>'
    if (trimmed.rfind("screen -s ", 0) == 0) {
        std::string procName = trim(trimmed.substr(10));
        if (procName.empty()) {
            std::cout << "  Error: Please specify a process name. Usage: screen -s <name>\n";
        } else {
            handleScreen_s(procName);
        }
        return true;
    }

    if (trimmed == "screen -ls" || trimmed == "screen-ls" || trimmed == "screen_ls") {
        handleScreen_ls();
        return true;
    }

    // Standard static command map routing
    auto it = commandMap.find(trimmed);
    if (it != commandMap.end()) {
        CommandHandler handler = it->second;
        (this->*handler)();
    } else {
        std::cout << "\n  Type one of the listed commands: initialize, screen -s <name>, scheduler-start, scheduler-stop, report-util, exit.\n\n";
    }

    return true;
}

// =========================================================================
// HANDLER IMPLEMENTATIONS
// =========================================================================

void CommandProcessor::printProcessScreenHeader(const std::shared_ptr<Process>& process) {
    if (!process) return;

    std::cout << "process name: " << process->getName() << "\n";
    std::cout << "ID: " << process->getPID() << "\n";
    std::cout << "Logs:\n";

    int assignedCore = process->getAssignedCore();
    std::cout << "(" << process->getStartedAtString() << ") Core "
              << (assignedCore >= 0 ? std::to_string(assignedCore) : "Waiting")
              << "\n";

    std::cout << "\nCurrent instruction line: " << process->getCurrentInstructionLine() << "\n";
    std::cout << "lines of code: " << process->getTotalLines() << "\n";
}

void CommandProcessor::handleScreen_s(const std::string& processName) {
    if (!isInitialized) {
        std::cout << "  Error: System must be initialized before creating a process.\n";
        return;
    }

    // Check if process already exists to re-enter its screen
    auto it = processMap.find(processName);
    if (it != processMap.end()) {
        currentAttachedProcess = it->second;
        clearScreen();
        std::cout << "[Re-entered Process Screen: " << processName << "]\n";
        printProcessScreenHeader(currentAttachedProcess);
        return;
    }

    // Generate random instruction limit between config boundaries
    const Config& config = configManager.getConfig();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(config.minIns, config.maxIns);
    uint32_t totalInstructions = dist(gen);

    // Construct the new Process object
    // Note: Modify this instantiation parameters to match your actual 'Process' constructor layout
    // Change this line:
    auto newProc = std::make_shared<Process>(nextPID++, processName, totalInstructions);
    
    // Track it locally and dispatch to the staged scheduler ready queue
    processMap[processName] = newProc;
    if (activeScheduler) {
        activeScheduler->addProcess(newProc);
    }

    // Change current terminal target context and draw screen UI
    currentAttachedProcess = newProc;
    clearScreen();
    printProcessScreenHeader(currentAttachedProcess);
}

void CommandProcessor::handleProcessSMI() {
    if (!currentAttachedProcess) return;

    std::cout << "\n================ PROCESS INFO ================\n";
    std::cout << "  Process Name       : " << currentAttachedProcess->getName() << "\n";
    std::cout << "  ID (PID)           : " << currentAttachedProcess->getPID() << "\n";
    std::cout << "  Started At         : " << currentAttachedProcess->getStartedAtString() << "\n";

    int assignedCore = currentAttachedProcess->getAssignedCore();
    std::cout << "  Assigned Core      : "
              << (assignedCore >= 0 ? std::to_string(assignedCore) : "Waiting")
              << "\n";
    std::cout << "  Current Instruction: " << currentAttachedProcess->getCurrentInstructionLine() << "\n";
    std::cout << "  Lines in Current Block: " << currentAttachedProcess->getCurrentFrameInstructionCount() << "\n";
    
    // Status / Finish check
    if (currentAttachedProcess->isFinished()) {
        std::cout << "  Status             : Finished!\n";
    } else {
        std::cout << "  Status             : Running / Staged\n";
    }

    // Print progress and log history metric values 
    std::cout << "  Progress           : " << currentAttachedProcess->getLinesExecuted() 
              << " / " << currentAttachedProcess->getTotalLines() << " instructions.\n";
    std::cout << "--------------------- LOGS ---------------------\n";
    
    // Call your actual log tracker to print out what just executed!
    currentAttachedProcess->printExecutionLogs(); 
    std::cout << "================================================\n\n";
}

void CommandProcessor::handleInitialize() {
    std::cout << "\n  [initialize] Reading configuration from 'config.txt'...\n";
    
    if (configManager.loadConfig("config.txt")) {
        const Config& config = configManager.getConfig();
        
        if (config.scheduler == "fcfs") {
            activeScheduler = std::make_unique<FCFSScheduler>(config.numCpu, config.delayPerExec);
            isInitialized = true;
            std::cout << "  [System] FCFS Scheduler successfully allocated and staged.\n";
            
            // =========================================================================
            // SPIN UP BACKGROUND THREAD IMMEDIATELY UPON INITIALIZATION
            // =========================================================================
            std::cout << "  [System] Activating background CPU execution cores...\n";
            schedulerWorkerThread = std::thread(&Scheduler::run, activeScheduler.get());
            schedulerWorkerThread.detach(); 
            // =========================================================================
        } 
        else if (config.scheduler == "rr") {
            activeScheduler = std::make_unique<RRScheduler>(config.quantumCycles, config.numCpu, config.delayPerExec);
            isInitialized = true;
            std::cout << "  [System] Round Robin Scheduler successfully allocated and staged.\n";
            
            // Do the same here if RRScheduler implements run()
            schedulerWorkerThread = std::thread(&Scheduler::run, activeScheduler.get());
            schedulerWorkerThread.detach();
        }
    } else {
        std::cerr << "  Error: Could not open or parse 'config.txt'.\n\n";
    }
}

void CommandProcessor::handleScreen_ls() {
    if (!isInitialized) {
        std::cout << "  Error: System must be initialized before listing process screens.\n";
        return;
    }

    const Config& config = configManager.getConfig();
    int totalCores = config.numCpu;
    int usedCores = 0;
    int runningProcesses = 0;
    int finishedProcesses = 0;

    for (const auto& entry : processMap) {
        const auto& process = entry.second;
        if (process && process->getAssignedCore() >= 0) {
            ++usedCores;
        }
        if (process && process->isFinished()) {
            ++finishedProcesses;
        } else {
            ++runningProcesses;
        }
    }

    double cpuUtilization = totalCores > 0 ? (100.0 * usedCores / totalCores) : 0.0;

    std::cout << "\n  [screen-ls] Process Screens\n";
    std::cout << "  CPU Utilization : " << std::fixed << std::setprecision(1) << cpuUtilization
              << "% (" << usedCores << "/" << totalCores << " cores used)\n";
    std::cout << "  Cores Used      : " << usedCores << "\n";
    std::cout << "  Cores Available : " << (totalCores - usedCores) << "\n";
    std::cout << "  Running Processes: " << runningProcesses << "\n";
    std::cout << "  Finished Processes: " << finishedProcesses << "\n";
    std::cout << "  ----------------------------------------\n";

    if (processMap.empty()) {
        std::cout << "  (No process screens have been created yet.)\n";
        return;
    }

    std::cout << "  PID | Name           | State      | Core | Last Updated\n";
    for (const auto& entry : processMap) {
        const auto& process = entry.second;
        if (!process) continue;

        std::string state = "READY";
        switch (process->getState()) {
            case Process::RUNNING: state = "RUNNING"; break;
            case Process::WAITING: state = "WAITING"; break;
            case Process::FINISHED: state = "FINISHED"; break;
            default: state = "READY"; break;
        }

        std::cout << "  " << process->getPID()
                  << " | " << std::setw(15) << std::left << process->getName() << std::right
                  << " | " << std::setw(10) << state
                  << " | " << std::setw(4) << (process->getAssignedCore() >= 0 ? std::to_string(process->getAssignedCore()) : "-")
                  << " | " << process->getLastUpdatedString() << "\n";
    }
}

void CommandProcessor::handleReportUtil() {
    if (!isInitialized) {
        std::cout << "  Error: System must be initialized before generating a report.\n";
        return;
    }

    const Config& config = configManager.getConfig();
    int totalCores = config.numCpu;
    int usedCores = 0;
    int runningProcesses = 0;
    int finishedProcesses = 0;

    for (const auto& entry : processMap) {
        const auto& process = entry.second;
        if (process && process->getAssignedCore() >= 0) {
            ++usedCores;
        }
        if (process && process->isFinished()) {
            ++finishedProcesses;
        } else {
            ++runningProcesses;
        }
    }

    double cpuUtilization = totalCores > 0 ? (100.0 * usedCores / totalCores) : 0.0;

    std::ofstream logFile("csopesy-log.txt", std::ios::app);
    if (!logFile.is_open()) {
        std::cout << "  Error: Could not open or create 'csopesy-log.txt'.\n";
        return;
    }

    logFile << "CSOPESY Process Report\n";
    logFile << "==============================================\n";
    logFile << "CPU Utilization : " << std::fixed << std::setprecision(1) << cpuUtilization
            << "% (" << usedCores << "/" << totalCores << " cores used)\n";
    logFile << "Cores Used      : " << usedCores << "\n";
    logFile << "Cores Available : " << (totalCores - usedCores) << "\n";
    logFile << "Running Processes: " << runningProcesses << "\n";
    logFile << "Finished Processes: " << finishedProcesses << "\n";
    logFile << "----------------------------------------------\n";

    if (processMap.empty()) {
        logFile << "(No process screens have been created yet.)\n";
    } else {
        logFile << "PID | Name           | State      | Core | Last Updated\n";
        for (const auto& entry : processMap) {
            const auto& process = entry.second;
            if (!process) continue;

            std::string state = "READY";
            switch (process->getState()) {
                case Process::RUNNING:  state = "RUNNING";  break;
                case Process::WAITING:  state = "WAITING";  break;
                case Process::FINISHED: state = "FINISHED"; break;
                default:                state = "READY";    break;
            }

            logFile << process->getPID()
                    << " | " << std::setw(15) << std::left << process->getName() << std::right
                    << " | " << std::setw(10) << state
                    << " | " << std::setw(4) << (process->getAssignedCore() >= 0 ? std::to_string(process->getAssignedCore()) : "-")
                    << " | " << process->getLastUpdatedString() << "\n";
        }
    }

    logFile << "==============================================\n";
    logFile.close();

    std::cout << "\n  [report-util] Report saved to 'csopesy-log.txt'.\n";
}

void CommandProcessor::handleScreen_r()    { std::cout << "\n  [screen_r] command recognized.\n"; }
void CommandProcessor::handleSchedulerStart() { std::cout << "\n  [scheduler-start] command recognized.\n"; }
void CommandProcessor::handleSchedulerStop()  { std::cout << "\n  [scheduler-stop] command recognized.\n"; }
void CommandProcessor::handleHelp() {
    std::cout << "\n  Available commands: initialize, screen -s <name>, screen -ls, scheduler-start, scheduler-stop, report-util, exit\n";
}