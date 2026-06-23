#include "CommandProcessor.h"
#include <iostream>
#include <algorithm>

CommandProcessor::CommandProcessor() {
    // Register your commands mapping strings to member function pointers
    commandMap["initialize"]      = &CommandProcessor::handleInitialize;
    commandMap["screen_s"]        = &CommandProcessor::handleScreen_s;
    commandMap["screen_ls"]       = &CommandProcessor::handleScreen_ls;
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

bool CommandProcessor::execute(const std::string& input) {
    std::string trimmed = trim(input);
    
    if (trimmed.empty()) {
        return true; 
    }

    // Explicit hardcoded OOP intercept for exit
    if (trimmed == "exit") {
        std::cout << "\n  exiting...\n";
        return false;
    }

    // Look up the command in our map
    auto it = commandMap.find(trimmed);
    if (it != commandMap.end()) {
        // Syntax to call a member function pointer on 'this' object
        CommandHandler handler = it->second;
        (this->*handler)();
    } else {
        std::cout << "\n  Type one of the listed commands: initialize, screen, scheduler-start, scheduler-stop, report-util, exit.\n\n";
    }

    return true;
}

// Handlers implementation
void CommandProcessor::handleInitialize() {
    std::cout << "\n  [initialize] command recognized. Doing something.\n";
}

void CommandProcessor::handleScreen_s() {
    std::cout << "\n  [screen] command recognized. Doing something.\n";
}

void CommandProcessor::handleScreen_ls() {
    std::cout << "\n  [screen] command recognized. Doing something.\n";
}

void CommandProcessor::handleSchedulerStart() {
    std::cout << "\n  [scheduler-start] command recognized. Doing something.\n";
}

void CommandProcessor::handleSchedulerStop() {
    std::cout << "\n  [scheduler-stop] command recognized. Doing something.\n";
}

void CommandProcessor::handleReportUtil() {
    std::cout << "\n  [report-util] command recognized. Doing something.\n";
}

void CommandProcessor::handleHelp() {
    std::cout << "\n  Available commands: initialize, screen, scheduler-start, scheduler-stop, report-util, exit\n";
}