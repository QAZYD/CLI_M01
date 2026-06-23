#include "SymbolTable.h"

bool SymbolTable::contains(const std::string& name) const {
    return table.find(name) != table.end();
}

void SymbolTable::remove(const std::string& name) {
    table.erase(name);
}

// Storing a value automatically handles type updating via std::variant
void SymbolTable::set( const std::string& name, uint16_t value)
{
    table[name]=value;
}

VariableValue SymbolTable::get(const std::string& name) const {
    auto it = table.find(name);
    if (it != table.end()) {
        return it->second;
    }
    // Throw an error if a command tries to read a variable that doesn't exist
    throw std::runtime_error("Error: Variable '" + name + "' undefined.");
}