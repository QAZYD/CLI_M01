#pragma once

#include <string>
#include <unordered_map>
#include <cstdint> 

class SymbolTable {
public:
    // Checkers and Utilities
    bool contains(const std::string& name) const;
    void remove(const std::string& name);

    // Simplified Setter for uint16_t
    void set(const std::string& name, uint16_t value);

    // Getter that implements the "auto-declare to 0" rule
    uint16_t get(const std::string& name);

private:
    // Simple map: variable name -> 16-bit unsigned integer
    std::unordered_map<std::string, uint16_t> table;
};