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
    std::cout << "\033[2J\033[1;1H"; // ANSI escape code to clear terminal
}

bool CommandProcessor::execute(const std::string& input) {
    std::string trimmed = trim(input);
    if (trimmed.empty()) return true;

    // CONTEXT A: INSIDE A PROCESS SCREEN
    if (currentAttachedProcess != nullptr) {
        if (trimmed == "process-smi") handleProcessSMI();
        else if (trimmed == "exit") {
            currentAttachedProcess = nullptr;
            clearScreen();
            std::cout << "  Returned to Main Menu Console.\n";
        } else {
            std::cout << "  Invalid Command inside Process Screen. Options: 'process-smi', 'exit'\n";
        }
        return true;
    }

    // CONTEXT B: MAIN MENU CONSOLE
    if (trimmed == "exit") {
        std::cout << "\n  exiting...\n";
        return false;
    }

    // Dynamic Parsing Intercept: screen -s <name>
    if (trimmed.rfind("screen -s ", 0) == 0) {
        std::string procName = trim(trimmed.substr(10));
        if (procName.empty()) std::cout << "  Error: Please specify a process name. Usage: screen -s <name>\n";
        else handleScreen_s(procName);
        return true;
    }

    if (trimmed == "screen -ls" || trimmed == "screen-ls" || trimmed == "screen_ls") {
        handleScreen_ls();
        return true;
    }

    // Standard static command map routing
    auto it = commandMap.find(trimmed);
    if (it != commandMap.end()) {
        (this->*(it->second))();
    } else {
        std::cout << "\n  Type one of the listed commands: initialize, screen -s <name>, scheduler-start, scheduler-stop, report-util, exit.\n\n";
    }
    return true;
}

void CommandProcessor::printProcessScreenHeader(const std::shared_ptr<Process>& process) {
    if (!process) return;

    int core = process->getAssignedCore();
    std::cout << "process name: " << process->getName() << "\n"
              << "ID: " << process->getPID() << "\n"
              << "Logs:\n"
              << "(" << process->getStartedAtString() << ") Core " << (core >= 0 ? std::to_string(core) : "Waiting") << "\n\n"
              << "Current instruction line: " << process->getCurrentInstructionLine() << "\n"
              << "lines of code: " << process->getTotalLines() << "\n";
}

void CommandProcessor::handleScreen_s(const std::string& processName) {
    if (!isInitialized) {
        std::cout << "  Error: System must be initialized before creating a process.\n";
        return;
    }

    auto it = processMap.find(processName);
    if (it != processMap.end()) {
        currentAttachedProcess = it->second;
        clearScreen();
        std::cout << "[Re-entered Process Screen: " << processName << "]\n";
        printProcessScreenHeader(currentAttachedProcess);
        return;
    }

    const Config& config = configManager.getConfig();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(config.minIns, config.maxIns);

    auto newProc = std::make_shared<Process>(nextPID++, processName, dist(gen));
    processMap[processName] = newProc;
    
    if (activeScheduler) activeScheduler->addProcess(newProc);

    currentAttachedProcess = newProc;
    clearScreen();
    printProcessScreenHeader(currentAttachedProcess);
}

void CommandProcessor::handleProcessSMI() {
    if (!currentAttachedProcess) return;

    int core = currentAttachedProcess->getAssignedCore();
    std::cout << "\n================ PROCESS INFO ================\n"
              << "  Process Name       : " << currentAttachedProcess->getName() << "\n"
              << "  ID (PID)           : " << currentAttachedProcess->getPID() << "\n"
              << "  Started At         : " << currentAttachedProcess->getStartedAtString() << "\n"
              << "  Assigned Core      : " << (core >= 0 ? std::to_string(core) : "Waiting") << "\n"
              << "  Current Instruction: " << currentAttachedProcess->getCurrentInstructionLine() << "\n"
              << "  Lines in Current Block: " << currentAttachedProcess->getCurrentFrameInstructionCount() << "\n"
              << "  Status             : " << (currentAttachedProcess->isFinished() ? "Finished!" : "Running / Staged") << "\n"
              << "  Progress           : " << currentAttachedProcess->getLinesExecuted() << " / " << currentAttachedProcess->getTotalLines() << " instructions.\n"
              << "--------------------- LOGS ---------------------\n";
    
    currentAttachedProcess->printExecutionLogs(); 
    std::cout << "================================================\n\n";
}

void CommandProcessor::handleInitialize() {
    std::cout << "\n  [initialize] Reading configuration from 'config.txt'...\n";
    
    if (!configManager.loadConfig("config.txt")) {
        std::cerr << "  Error: Could not open or parse 'config.txt'.\n\n";
        return;
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
    const Config& config = configManager.getConfig();
    if (config.scheduler == "fcfs") {
        activeScheduler = std::make_unique<FCFSScheduler>(config.numCpu, config.delayPerExec);
        isInitialized = true;
        std::cout << "  [System] FCFS Scheduler successfully allocated and staged.\n";
    } else if (config.scheduler == "rr") {
        activeScheduler = std::make_unique<RRScheduler>(config.quantumCycles, config.numCpu, config.delayPerExec);
        isInitialized = true;
        std::cout << "  [System] Round Robin Scheduler successfully allocated and staged.\n";
    }

    if (isInitialized) {
        std::cout << "  [System] Activating background CPU execution cores...\n";
        schedulerWorkerThread = std::thread(&Scheduler::run, activeScheduler.get());
        schedulerWorkerThread.detach(); 
    }
}