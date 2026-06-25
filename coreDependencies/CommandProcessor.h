#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <thread> 
#include <iostream> // Added to support inline std::cout stubs
#include "configManager.h"
#include "Scheduler.h"
#include "ProcessControl.h" 

class CommandProcessor {
public:
    CommandProcessor();
    bool execute(const std::string& input);

private:
    typedef void (CommandProcessor::*CommandHandler)();
    std::unordered_map<std::string, CommandHandler> commandMap;
    std::thread schedulerWorkerThread; 
    std::string trim(const std::string& str);
    void clearScreen();

    // Core States & Dependencies
    ConfigManager configManager;
    bool isInitialized = false;
    std::unique_ptr<Scheduler> activeScheduler = nullptr;

    // Screen & Process Context Tracking
    std::shared_ptr<Process> currentAttachedProcess = nullptr; 
    std::unordered_map<std::string, std::shared_ptr<Process>> processMap;
    uint32_t nextPID = 1;

    // Command Handlers
    void handleInitialize();
    void handleScreen_s(const std::string& processName); 
    
    // Inline Stub Implementations (Moved from .cpp to trim code footprint)
    void handleScreen_ls()       { std::cout << "\n  [screen_ls] command recognized.\n"; }
    void handleScreen_r()        { std::cout << "\n  [screen_r] command recognized.\n"; }
    void handleSchedulerStart()  { std::cout << "\n  [scheduler-start] command recognized.\n"; }
    void handleSchedulerStop()   { std::cout << "\n  [scheduler-stop] command recognized.\n"; }
    void handleReportUtil()      { std::cout << "\n  [report-util] command recognized.\n"; }
    void handleHelp() {
        std::cout << "\n  Available commands: initialize, screen -s <name>, scheduler-start, scheduler-stop, report-util, exit\n";
    }
    
    // Process Screen Specific Handlers
    void handleProcessSMI();
    void printProcessScreenHeader(const std::shared_ptr<Process>& process);
};