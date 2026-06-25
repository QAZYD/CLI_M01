#include "coreDependencies/SymbolTable.h"

bool SymbolTable::contains(const std::string& name) const {
    return table.find(name) != table.end();
}

void SymbolTable::remove(const std::string& name) {
    table.erase(name);
}

void SymbolTable::set(const std::string& name, uint16_t value) {
    table[name] = value; 
}

uint16_t SymbolTable::get(const std::string& name) {
    // If the variable doesn't exist, automatically declare it with 0
    if (!contains(name)) {
        table[name] = 0;
    }
    return table[name];
}