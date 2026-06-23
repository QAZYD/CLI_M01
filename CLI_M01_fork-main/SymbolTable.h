#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <cstdint>

// replaced int char float, only uses uint16
using VariableValue = uint16_t;

class SymbolTable {
public:
    // Checkers and Utilities
    bool contains(const std::string& name) const;
    void remove(const std::string& name);

    // replaced int char float setter
    void set(const std::string&, uint16_t);

    // Getters
    VariableValue get(const std::string& name) const;

private:
    std::unordered_map<std::string, VariableValue> table;
};