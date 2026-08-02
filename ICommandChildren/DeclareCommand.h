#pragma once
#include "../coreDependencies/ICommand.h"
#include "../coreDependencies/SymbolTable.h"
#include <string>
#include <cstdint>

class DeclareCommand : public ICommand {
private:
    std::string varName;
    uint16_t defaultValue;

public:
    DeclareCommand(std::string var, uint16_t val) 
        : varName(var), defaultValue(val) {}

    void execute(SymbolTable& table) override {
        table.set(varName, defaultValue);
    }

    void execute(Process& process, MemoryManager& memoryManager) override
    {
        // Intentionally empty.
        // Process::executeCurrentCommand() intercepts ForCommand
        // and pushes the loop frame instead of executing it directly.
    }

    // --- ADDED TOSTRING IMPLEMENTATION ---
    std::string toString() const override {
        return "DECLARE(" + varName + ", " + std::to_string(defaultValue) + ")";
    }
};