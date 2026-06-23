#include "ICommand.h"
#include <iostream>
#include <string>

// Command 1: Just prints text to the console
class PrintCommand : public ICommand {
private:
    std::string message;
public:
    PrintCommand(std::string msg) : message(msg) {}

    //modified params to match header
    void execute(Process& process) override {
        std::cout << "[Output]: " << message << std::endl;
    }
};

// Command 2: Performs a simple math operation
class MathCommand : public ICommand {
private:
    int& resultDestination; // References where to save the result
    int a, b;
public:
    MathCommand(int& dest, int val1, int val2) 
        : resultDestination(dest), a(val1), b(val2) {}

    //modified params to match header
    void execute(Process& process) override {
        resultDestination = a + b;
    }
};