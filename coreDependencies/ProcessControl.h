#pragma once
#include <memory>
#include <vector>
#include <string>
#include "SymbolTable.h"
#include "ICommand.h"

class Process {
public:
    enum ProcessState {
        READY,
        RUNNING,
        WAITING,
        FINISHED
    };

    // Tracks an isolated block of instructions (The main script or a loop body)
    struct ExecutionFrame {
        std::vector<std::shared_ptr<ICommand>> instructions;
        int pc = 0;
        int repeatsLeft = 1;
    };

    Process(int pid, std::string name);

    void addCommand(std::shared_ptr<ICommand> command);
    void executeCurrentCommand();
    void moveToNextLine();

    bool isFinished() const;
    int getPID() const;
    ProcessState getState() const;
    std::string getName() const;

    void setState(ProcessState State);
    SymbolTable& getSymbolTable();

    // Controls for flow interception
    void pushLoopFrame(const std::vector<std::shared_ptr<ICommand>>& instructions, int repeats);
    void sleep(int ticks);
    void decrementSleepTicks();

private:
    int pid;
    std::string name;
    ProcessState currentState;
    
    std::vector<ExecutionFrame> executionStack;
    bool isStackInitialized;

    std::vector<std::shared_ptr<ICommand>> commandList; 
    SymbolTable symbolTable;
    int sleepTicksRemaining;
};