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
    void handleInitialize();
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
    
    void handleScreen_s(const std::string& processName);
    void handleScreen_ls();
    void handleHelp();
    void handleReportUtil();
    void handleScreen_r();
    void handleSchedulerStart();
    void handleSchedulerStop(); 
    
    // Process Screen Specific Handlers
    void handleProcessSMI();
    void printProcessScreenHeader(const std::shared_ptr<Process>& process);
};