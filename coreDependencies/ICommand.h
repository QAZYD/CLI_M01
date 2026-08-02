#pragma once
#include <string>
#include "SymbolTable.h"

// Forward declarations
class Process;
class MemoryManager;

class ICommand {
public:
    virtual ~ICommand() = default;

    // 1. Used by basic commands (ADD, PRINT, DECLARE, etc.)
    virtual void execute(SymbolTable& table) {}

    // 2. Declaration ONLY (body lives in ICommand.cpp)
    virtual void execute(Process& process, MemoryManager& memoryManager);

    // 3. Pure virtual string representation
    virtual std::string toString() const = 0;
};