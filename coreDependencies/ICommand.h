#pragma once
#include "SymbolTable.h"

class Process;
class MemoryManager;

class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void execute(SymbolTable& table) = 0;

    // Pass Process and MemoryManager into execute[cite: 1]
    virtual void execute(Process& process, MemoryManager& memoryManager) = 0;
    virtual std::string toString() const = 0;
};