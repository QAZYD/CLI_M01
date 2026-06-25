#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <string>
#include <map>

class CommandProcessor {
public:
    CommandProcessor();
    
    // Processes the input string and returns false if the program should exit
    bool execute(const std::string& input);

private:
    // Typedef for internal command handler functions
    using CommandHandler = void (CommandProcessor::*)();

    // Map to link a string token to its corresponding member function
    std::map<std::string, CommandHandler> commandMap;

    // Command handlers
    void handleInitialize();
    void handleScreen_s();
    void handleScreen_r();
    void handleScreen_ls();
    void handleSchedulerStart();
    void handleSchedulerStop();
    void handleReportUtil();
    void handleHelp();

    // Helper utility
    std::string trim(const std::string& str);
};

#endif // COMMAND_PROCESSOR_H