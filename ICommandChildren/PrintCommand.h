#pragma once
#include "../coreDependencies/ICommand.h"
#include "../coreDependencies/SymbolTable.h"
#include <iostream>
#include <string>

class PrintCommand : public ICommand {
private:
    std::string literalMessage;
    std::string varName; // Pass empty string if there's no variable to print
    mutable std::string evaluatedMessage; // Stores evaluated result after execution

public:
    PrintCommand(std::string msg, std::string var = "") 
        : literalMessage(msg), varName(var), evaluatedMessage("") {}

    void execute(SymbolTable& table) override {
        evaluatedMessage = literalMessage;
        if (!varName.empty()) {
            if (table.contains(varName)) {
                evaluatedMessage += std::to_string(table.get(varName));
            } else {
                evaluatedMessage += "[UNDEFINED: " + varName + "]";
            }
        }
    }

    // --- TOSTRING IMPLEMENTATION ---
    std::string toString() const override {
        // If execution already occurred, return the evaluated string result
        if (!evaluatedMessage.empty()) {
            return evaluatedMessage;
        }

        // Un-executed fallback string representation
        if (!varName.empty()) {
            return literalMessage + " " + varName;
        }
        return literalMessage;
    }
};