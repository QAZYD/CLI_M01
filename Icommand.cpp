#include "ICommand.h"
#include <iostream>
#include <string>

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