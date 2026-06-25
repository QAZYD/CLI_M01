#pragma once
#include "../coreDependencies/ICommand.h"
#include "../coreDependencies/SymbolTable.h"
#include <string>
#include <cctype>
#include <algorithm>

class AddCommand : public ICommand {
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
    AddCommand(std::string dest, std::string op1, std::string op2) 
        : destVar(dest), operand1(op1), operand2(op2) {}

    void execute(SymbolTable& table) override {
        uint16_t val1 = resolveValue(operand1, table);
        uint16_t val2 = resolveValue(operand2, table);
        table.set(destVar, val1 + val2);
    }
};