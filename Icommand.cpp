#pragma once
#include "ICommand.h"
#include <iostream>
#include <cctype>
#include <string>
#include <cstdint>
#include <algorithm>

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
};

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

class AddCommand : public ICommand {
private:
    std::string destVar;
    std::string operand1;
    std::string operand2;

    // Helper: Returns raw number if it's numeric, otherwise looks it up in the table
    uint16_t resolveValue(const std::string& token, SymbolTable& table) {
        if (!token.empty() && std::all_of(token.begin(), token.end(), ::isdigit)) {
            return static_cast<uint16_t>(std::stoi(token));
        }
        return table.get(token); // Auto-declares to 0 if variable doesn't exist
    }

public:
    AddCommand(std::string dest, std::string op1, std::string op2) 
        : destVar(dest), operand1(op1), operand2(op2) {}

    void execute(SymbolTable& table) override {
        uint16_t val1 = resolveValue(operand1, table);
        uint16_t val2 = resolveValue(operand2, table);
        
        table.set(destVar, val1 + val2);
    }
};

class SubtractCommand : public ICommand {
private:
    std::string destVar;
    std::string operand1;
    std::string operand2;

    uint16_t resolveValue(const std::string& token, SymbolTable& table) {
        if (!token.empty() && std::all_of(token.begin(), token.end(), ::isdigit)) {
            return static_cast<uint16_t>(std::stoi(token));
        }
        return table.get(token);
    }

public:
    SubtractCommand(std::string dest, std::string op1, std::string op2) 
        : destVar(dest), operand1(op1), operand2(op2) {}

    void execute(SymbolTable& table) override {
        uint16_t val1 = resolveValue(operand1, table);
        uint16_t val2 = resolveValue(operand2, table);
        
        // Prevents underflow since uint16_t cannot be negative
        uint16_t result = (val1 > val2) ? (val1 - val2) : 0;
        
        table.set(destVar, result);
    }
};