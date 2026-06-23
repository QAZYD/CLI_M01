#include "ICommand.h"
#include "Process.h"
#include <iostream>
#include <string>
#include <cstdint>

// Command 1: Just prints text to the console
class PrintCommand : public ICommand {
private:
    std::string message;
public:
    PrintCommand(std::string msg) : message(msg) {}

    void execute() override {
        std::cout << "[Output]: " << message << std::endl;
    }
};

// Command 2: Declare 
class DeclareCommand : public ICommand {
private:
    std::string varName;
    uintptr_t value;
public:
    DeclareCommand(const std::string& name, uint16_t val) : varName(name), value(val) {}

    void execute() override { // Probably need to pass a process here or some other way to make this valid
        // process.getSymbolTable().set(varName, value); // Get the symboltable of the process there
    }
};

// Command 3: Addition
class AddCommand : public ICommand {
private:
    int& resultDestination; // References where to save the result
    int a, b;
public:
    AddCommand(int& dest, int val1, int val2) 
        : resultDestination(dest), a(val1), b(val2) {}

    void execute() override {
        resultDestination = a + b;
    }
};

// Command 4: Subtract
class SubCommand : public ICommand {
private:
    int& resultDestination; // References where to save the result
    int a, b;
public:
    SubCommand(int& dest, int val1, int val2) 
        : resultDestination(dest), a(val1), b(val2) {}

    void execute() override {
        resultDestination = a - b;
    }
};

// Command 5: Sleep
class SleepCommand : public ICommand {
private:
    uint8_t duration;
public:
    SleepCommand(uint8_t x) : duration(x) {}

    void execute() override { // Don't have the current stuff to implement properly
        // TODO: Some other stuff here like get the current tick
        // process.setState(Process::WAITING); 
        // TODO: Then after some times has pass, wake the process up
    }
};

// Command 6: performs a for-loop, given a set/array of instructions.
class ForCommand : public ICommand {
private:
    std::vector<std::shared_ptr<ICommand>> body;
    int repeat;
public:
    ForCommand(std::vector<std::shared_ptr<ICommand>> b, int r) : body(b), repeat(r) {}

    void execute() override {
        for (int i = 0; i < repeat; i++) {
            for (auto& cmd : body) {
                cmd->execute(); // Process here?
            }
        }
    }
};