#pragma once
#include "../coreDependencies/ICommand.h"
#include "../coreDependencies/SymbolTable.h"
#include <iostream>
#include <string>

class PrintCommand : public ICommand {
private:
    std::string literalMessage;
    std::string varName; // Pass empty string if there's no variable to print

public:
    PrintCommand(std::string msg, std::string var = "") 
        : literalMessage(msg), varName(var) {}

    void execute(SymbolTable& table) override {
        // FIX: Remove std::cout entirely.
        // The background thread will now execute this command silently.
        
    }

    void execute(Process& process, MemoryManager& memoryManager) override
    {
        // Intentionally empty.
        // Process::executeCurrentCommand() intercepts ForCommand
        // and pushes the loop frame instead of executing it directly.
    }

    // --- TOSTRING IMPLEMENTATION ---
    std::string toString() const override {
        if (!varName.empty()) {
            // Returns: "Hello world from <process_name>! v1" 
            return literalMessage + " " + varName;
        }
        return literalMessage;
    }
};