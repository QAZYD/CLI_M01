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

    // Constructor updated with an optional totalLines parameter for dummy generation
    Process(int pid, std::string name, int totalLines = 0);

    void addCommand(std::shared_ptr<ICommand> command);
    void executeCurrentCommand();
    void moveToNextLine();

    bool isFinished() const;
    int getPID() const;
    ProcessState getState() const;
    std::string getName() const;

    // New diagnostic metrics getters called by process-smi
    int getLinesExecuted() const;
    int getTotalLines() const;
    void printExecutionLogs() const;
    int getSleepTicksRemaining() const { return sleepTicksRemaining; }
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

    // New tracking variables for diagnostic readouts
    int linesExecuted;
    std::vector<std::string> logs;
};