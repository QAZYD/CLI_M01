#pragma once
#include "../coreDependencies/ICommand.h"
#include <vector>
#include <memory>
#include <string> // Added for std::string and std::to_string

class ForCommand : public ICommand {
private:
    std::vector<std::shared_ptr<ICommand>> innerInstructions;
    int repeats;
public:
    ForCommand(const std::vector<std::shared_ptr<ICommand>>& instrs, int repeatCount)
        : innerInstructions(instrs), repeats(repeatCount) {}

    // Satisfies the interface constraint; implementation is handled via interception
    void execute(SymbolTable& test) override {}

    const std::vector<std::shared_ptr<ICommand>>& getInstructions() const { return innerInstructions; }
    int getRepeats() const { return repeats; }

    // --- ADDED TOSTRING IMPLEMENTATION ---
    std::string toString() const override {
        return "FOR LOOP: REPEAT " + std::to_string(repeats) + " TIMES";
    }
};