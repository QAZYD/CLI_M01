#pragma once
#include "../coreDependencies/ICommand.h"
#include "MemoryManager.h"
#include "../coreDependencies/ProcessControl.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>

class ReadCommand : public ICommand {
private:
    std::string varName;
    uint16_t address;

public:
    // Direct uint16 constructor
    ReadCommand(std::string var, uint16_t addr) : varName(var), address(addr) {}

    // String constructor (Handles both hex "0x1000" and decimal "4096")
    ReadCommand(std::string var, const std::string& addrStr) : varName(var) {
        if (addrStr.find("0x") == 0 || addrStr.find("0X") == 0) {
            address = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
        } else {
            address = static_cast<uint16_t>(std::stoul(addrStr));
        }
    }

    void execute(Process& process, MemoryManager& memoryManager) override {
        uint16_t readValue = 0;

        // MemoryManager reads value (or sets MEMORY_VIOLATION if out-of-bounds)
        bool success = memoryManager.read_uint16(process, address, readValue);
        
        if (success) {
            // Store retrieved uint16 into variable in SymbolTable (respects 32-var cap & clamping)
            process.getSymbolTable().set(varName, readValue);
        }
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "READ " << varName << " 0x" << std::hex << std::uppercase << address;
        return ss.str();
    }
};