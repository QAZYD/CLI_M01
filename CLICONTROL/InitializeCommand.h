#pragma once

#include "../coreDependencies/ConfigManager.h"
#include <string>

class InitializeCommand {
private:
    ConfigManager configManager;
    bool isInitialized;
    const std::string configFilePath;

public:
    InitializeCommand(const std::string& filePath = "config.txt");

    // Executes the loading logic and returns success/failure
    bool execute();

    // Accessors for other systems to read the values later
    bool getIsInitialized() const;
    const Config& getConfig() const;
    
    // Prints out a clean overview of the parsed configurations
    void printConfigSummary() const;
};