#include "coreDependencies/SymbolTable.h"
#include <algorithm> // For std::min

bool SymbolTable::contains(const std::string& name) const {
    return table.find(name) != table.end();
}

void SymbolTable::remove(const std::string& name) {
    table.erase(name);
}

bool SymbolTable::declare(const std::string& name, uint32_t value) {
    // If variable already exists, update its value
    if (contains(name)) {
        set(name, value);
        return true;
    }

    // Capacity Check: Max 32 variables (64 bytes total)
    if (table.size() >= MAX_VARIABLES) {
        return false; // Limit reached: ignore declaration
    }

    // Clamp value between 0 and 65535 (max uint16)
    uint16_t clampedVal = static_cast<uint16_t>(std::min(value, 65535u));
    table[name] = clampedVal;
    return true;
}

void SymbolTable::set(const std::string& name, uint32_t value) {
    uint16_t clampedVal = static_cast<uint16_t>(std::min(value, 65535u));

    if (contains(name)) {
        table[name] = clampedVal;
    } else {
        // Delegate to declare() to respect the 32-variable limit
        declare(name, value);
    }
}

uint16_t SymbolTable::get(const std::string& name) {
    auto it = table.find(name);
    if (it != table.end()) {
        return it->second;
    }

    // "Auto-declare to 0" rule — ONLY if under the 32-variable limit
    if (table.size() < MAX_VARIABLES) {
        table[name] = 0;
        return 0;
    }

    // If capacity is full and variable is uninitialized, return 0 without inserting
    return 0;
}