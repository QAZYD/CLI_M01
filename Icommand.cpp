#include "coreDependencies/ICommand.h"
#include "coreDependencies/ProcessControl.h" // Full definition of Process needed here

void ICommand::execute(Process& process, MemoryManager& memoryManager) {
    // Default behavior: Forward execution to legacy SymbolTable overload
    execute(process.getSymbolTable());
}