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

    uint8_t getTicks() const { return ticks; }

    // --- ADDED TOSTRING IMPLEMENTATION ---
    std::string toString() const override {
        return "SLEEP " + std::to_string(static_cast<int>(ticks));
    }
};