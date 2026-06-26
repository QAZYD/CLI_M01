#pragma once
#include "SymbolTable.h"

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(SymbolTable& table) = 0;
    virtual std::string toString() const = 0;
};