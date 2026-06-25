#pragma once
#include "../coreDependencies/ICommand.h"
#include <cstdint>

class SleepCommand : public ICommand {
private:
    uint8_t ticks;
public:
    SleepCommand(uint8_t sleepTicks) : ticks(sleepTicks) {}

    // Satisfies the interface constraint; implementation is handled via interception
    void execute(SymbolTable& test) override {} 

    uint8_t getTicks() const { return ticks; }
};