#pragma once

#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>

// Define our types clearly. std::variant holds EXACTLY one of these at a time.
using VariableValue = std::variant<int, char, float>;

class SymbolTable {
public:
    // Checkers and Utilities
    bool contains(const std::string& name) const;
    void remove(const std::string& name);

    // Setters (Overloaded for different types)
    void set(const std::string& name, int value);
    void set(const std::string& name, char value);
    void set(const std::string& name, float value);

    // Getters
    VariableValue get(const std::string& name) const;

private:
    std::unordered_map<std::string, VariableValue> table;
};