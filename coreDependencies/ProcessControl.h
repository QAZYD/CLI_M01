#pragma once
#include <memory>
#include <vector>
#include <string>
#include <chrono>
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

    // Lifecycle & Core Execution
    Process(int pid, std::string name, int totalLines = 0);
    void addCommand(std::shared_ptr<ICommand> command);
    void executeCurrentCommand();
    void moveToNextLine();

    // Fast Inline Getters & Setters
    int getPID() const { return pid; }
    std::string getName() const { return name; }
    ProcessState getState() const { return currentState; }
    void setState(ProcessState state) { currentState = state; }
    
    bool isFinished() const { return currentState == FINISHED || (isStackInitialized && executionStack.empty()); }
    SymbolTable& getSymbolTable() { return symbolTable; }

    // Diagnostic Metrics Getters (Simple Inlines)
    int getLinesExecuted() const { return linesExecuted; }
    int getTotalLines() const { return totalInstructions; }
    int getSleepTicksRemaining() const { return sleepTicksRemaining; }
    int getAssignedCore() const { return assignedCore; }
    void setAssignedCore(int core) { assignedCore = core; }

    // Complex Metrics (Implemented in .cpp)
    std::string getStartedAtString() const;
    int getCurrentInstructionLine() const;
    int getCurrentFrameInstructionCount() const;
    void printExecutionLogs() const;

    // Flow Interception Controls
    void pushLoopFrame(const std::vector<std::shared_ptr<ICommand>>& instructions, int repeats);
    void sleep(int ticks);
    void decrementSleepTicks();

private:
    // Identity & State
    int pid;
    std::string name;
    ProcessState currentState;
    
    // Core VM / Execution Stack
    std::vector<ExecutionFrame> executionStack;
    std::vector<std::shared_ptr<ICommand>> commandList; 
    SymbolTable symbolTable;
    bool isStackInitialized;

    // Execution Diagnostics & Tracking
    int totalInstructions;
    int linesExecuted;
    int sleepTicksRemaining;
    int assignedCore;
    std::vector<std::string> logs;
    std::chrono::system_clock::time_point startedAt;
};