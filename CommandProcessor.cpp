#include "coreDependencies/CommandProcessor.h"
#include "coreDependencies/FCFSScheduler.h"
#include "coreDependencies/RRScheduler.h"
#include <iostream>
#include <algorithm>
#include <random>

CommandProcessor::CommandProcessor() {
    commandMap["initialize"]      = &CommandProcessor::handleInitialize;
    commandMap["screen_ls"]       = &CommandProcessor::handleScreen_ls;
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