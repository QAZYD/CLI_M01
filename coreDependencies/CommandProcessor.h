#pragma once
#include <string>
#include <unordered_map>

class CommandProcessor {
public:
    CommandProcessor();
    bool execute(const std::string& input);

private:
    // Typedef for a member function pointer on CommandProcessor that takes no args and returns void
    typedef void (CommandProcessor::*CommandHandler)();
    std::unordered_map<std::string, CommandHandler> commandMap;

    std::string trim(const std::string& str);

    // Command Handler Declarations
    void handleInitialize();
    void handleScreen_s();
    void handleScreen_ls();
    void handleScreen_r();
    void handleSchedulerStart();
    void handleSchedulerStop();
    void handleReportUtil();
    void handleHelp();
};