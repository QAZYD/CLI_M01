#pragma once
#include "../coreDependencies/ICommand.h"
#include <cstdint>
#include <string> // Added for std::string and std::to_string

class SleepCommand : public ICommand {
private:
    uint8_t ticks;
public:
    SleepCommand(uint8_t sleepTicks) : ticks(sleepTicks) {}

    // Satisfies the interface constraint; implementation is handled via interception
    void execute(SymbolTable& test) override {} 

    void execute(Process& process, MemoryManager& memoryManager) override
    {
        // Intentionally empty.
        // Process::executeCurrentCommand() intercepts ForCommand
        // and pushes the loop frame instead of executing it directly.
    }

    uint8_t getTicks() const { return ticks; }

    // --- ADDED TOSTRING IMPLEMENTATION ---
    std::string toString() const override {
        return "SLEEP " + std::to_string(static_cast<int>(ticks));
    }
};