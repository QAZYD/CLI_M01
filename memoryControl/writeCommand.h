#include "../coreDependencies/ICommand.h"
#include "MemoryManager.h"
#include "../coreDependencies/ProcessControl.h"

class WriteCommand : public ICommand {
private:
    uint16_t address;
    std::string valueOrVar;

public:
    WriteCommand(uint16_t addr, std::string val) : address(addr), valueOrVar(val) {}

    void execute(Process& process, MemoryManager& memoryManager) override {
        uint16_t valToWrite = 0;

        // Check if value is a variable in SymbolTable or a literal uint16[cite: 1]
        if (process.getSymbolTable().contains(valueOrVar)) {
            valToWrite = process.getSymbolTable().get(valueOrVar);
        } else {
            valToWrite = static_cast<uint16_t>(std::stoi(valueOrVar));
        }

        // Perform memory write via MemoryManager[cite: 1]
        bool success = memoryManager.write_uint16(process, address, valToWrite);
        
        if (!success) {
            // Out-of-bounds error occurred -> process handles shutdown[cite: 1]
            process.triggerMemoryViolation(address, process.captureCurrentTimestamp());
        }
    }

    std::string toString() const override {
        return "WRITE " + std::to_string(address) + " " + valueOrVar;
    }
};