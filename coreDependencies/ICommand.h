#pragma once
#include "SymbolTable.h"

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(SymbolTable& table) = 0;
};