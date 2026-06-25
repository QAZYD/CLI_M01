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
        std::cout << "  [Switched to Existing Process Screen: " << processName << "]\n";
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
    std::cout << "  [Created & Switched to Process Screen: " << processName << "]\n";
}

void CommandProcessor::handleProcessSMI() {
    if (!currentAttachedProcess) return;

    std::cout << "\n================ PROCESS INFO ================\n";
    std::cout << "  Process Name : " << currentAttachedProcess->getName() << "\n";
    std::cout << "  ID (PID)     : " << currentAttachedProcess->getPID() << "\n";
    
    // Status / Finish check
    if (currentAttachedProcess->isFinished()) {
        std::cout << "  Status       : Finished!\n";
    } else {
        std::cout << "  Status       : Running / Staged\n";
    }

    // Print progress and log history metric values 
    std::cout << "  Progress     : " << currentAttachedProcess->getLinesExecuted() 
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

void CommandProcessor::handleScreen_ls()   { std::cout << "\n  [screen_ls] command recognized.\n"; }
void CommandProcessor::handleScreen_r()    { std::cout << "\n  [screen_r] command recognized.\n"; }
void CommandProcessor::handleSchedulerStart() { std::cout << "\n  [scheduler-start] command recognized.\n"; }
void CommandProcessor::handleSchedulerStop()  { std::cout << "\n  [scheduler-stop] command recognized.\n"; }
void CommandProcessor::handleReportUtil()  { std::cout << "\n  [report-util] command recognized.\n"; }
void CommandProcessor::handleHelp() {
    std::cout << "\n  Available commands: initialize, screen -s <name>, scheduler-start, scheduler-stop, report-util, exit\n";
}