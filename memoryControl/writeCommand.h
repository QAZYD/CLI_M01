#pragma once
#include "../coreDependencies/ICommand.h"
#include "MemoryManager.h"
#include "../coreDependencies/ProcessControl.h"
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <algorithm>

class WriteCommand : public ICommand {
private:
    uint16_t address;
    std::string valueOrVar;

public:
    // Direct uint16 address constructor
    WriteCommand(uint16_t addr, std::string val) : address(addr), valueOrVar(val) {}

    // String constructor (Handles both hex "0x2000" and decimal "8192")
    WriteCommand(const std::string& addrStr, std::string val) : valueOrVar(val) {
        if (addrStr.find("0x") == 0 || addrStr.find("0X") == 0) {
            address = static_cast<uint16_t>(std::stoul(addrStr, nullptr, 16));
        } else {
            address = static_cast<uint16_t>(std::stoul(addrStr));
        }
    }

    void execute(Process& process, MemoryManager& memoryManager) override {
        uint16_t valToWrite = 0;

        // Check if value is a variable in SymbolTable or a literal uint16
        if (process.getSymbolTable().contains(valueOrVar)) {
            valToWrite = process.getSymbolTable().get(valueOrVar);
        } else {
            try {
                unsigned long parsedVal = std::stoul(valueOrVar);
                valToWrite = static_cast<uint16_t>(std::min(parsedVal, 65535ul)); // Clamping to max uint16
            } catch (...) {
                valToWrite = 0;
            }
        }

        // MemoryManager writes value (or sets MEMORY_VIOLATION if out-of-bounds)
        memoryManager.write_uint16(process, address, valToWrite);
    }

    std::string toString() const override {
        std::stringstream ss;
        ss << "WRITE 0x" << std::hex << std::uppercase << address << " " << valueOrVar;
        return ss.str();
    }
};  