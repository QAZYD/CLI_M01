#pragma once
#include "ICommand.h"
#include "SymbolTable.h"
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
        // NOTE: add an 'if' statement here once screen has been implemented 
        // to check if the user is currently looking at this process's screen.
        
        std::cout << literalMessage;
        if (!varName.empty()) {
            // table.get automatically defaults to 0 if not declared!
            std::cout << table.get(varName); 
        }
        std::cout << std::endl;
    }
};