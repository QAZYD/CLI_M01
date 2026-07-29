#include "../coreDependencies/ICommand.h"
#include "MemoryManager.h"
#include "../coreDependencies/ProcessControl.h"

class ReadCommand : public ICommand {
private:
    std::string varName;
    uint16_t address;

public:
    ReadCommand(std::string var, uint16_t addr) : varName(var), address(addr) {}

    void execute(Process& process, MemoryManager& memoryManager) override {
        uint16_t readValue = 0;

        // Perform memory read via MemoryManager[cite: 1]
        bool success = memoryManager.read_uint16(process, address, readValue);
        
        if (!success) {
            process.triggerMemoryViolation(address, process.captureCurrentTimestamp());
            return;
        }

        // Store retrieved uint16 into variable in SymbolTable[cite: 1]
        process.getSymbolTable().set(varName, readValue);
    }

    std::string toString() const override {
        return "READ " + varName + " " + std::to_string(address);
    }
};