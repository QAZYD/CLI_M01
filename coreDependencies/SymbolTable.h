#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>
#include <algorithm>

class SymbolTable {
public:
    // Fixed limit: 64 bytes total / 2 bytes per uint16 = 32 variables maximum
    static constexpr size_t MAX_VARIABLES = 32;

    // Checkers and Utilities
    bool contains(const std::string& name) const;
    void remove(const std::string& name);
    size_t size() const;
    bool isFull() const;

    // Explicit Declaration (Returns false if table is full and declaration is ignored)
    bool declare(const std::string& name, uint32_t value = 0);

    // Setter (Clamps values between 0 and 65535)
    void set(const std::string& name, uint32_t value);

    // Getter that implements the "auto-declare to 0" rule (respecting MAX_VARIABLES)
    uint16_t get(const std::string& name);

private:
    std::unordered_map<std::string, uint16_t> table;
};