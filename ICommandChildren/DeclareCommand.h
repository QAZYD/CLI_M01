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
};