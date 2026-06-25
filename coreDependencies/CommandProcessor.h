// coreDependencies/CommandProcessor.h
#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "configManager.h"
#include "Scheduler.h"
#include "ProcessControl.h" // Ensure this contains your Process definition

class CommandProcessor {
public:
    CommandProcessor();
    bool execute(const std::string& input);

private:
    typedef void (CommandProcessor::*CommandHandler)();
    std::unordered_map<std::string, CommandHandler> commandMap;

    std::string trim(const std::string& str);
    void clearScreen();

    // Core States & Dependencies
    ConfigManager configManager;
    bool isInitialized = false;
    std::unique_ptr<Scheduler> activeScheduler = nullptr;

    // Screen & Process Context Tracking
    std::shared_ptr<Process> currentAttachedProcess = nullptr; // Null = Main Menu
    std::unordered_map<std::string, std::shared_ptr<Process>> processMap;
    uint32_t nextPID = 1;

    // Command Handlers
    void handleInitialize();
    void handleScreen_s(const std::string& processName); // Modified to take an argument
    void handleScreen_ls();
    void handleScreen_r();
    void handleSchedulerStart();
    void handleSchedulerStop();
    void handleReportUtil();
    void handleHelp();
    
    // Process Screen Specific Handlers
    void handleProcessSMI();
};