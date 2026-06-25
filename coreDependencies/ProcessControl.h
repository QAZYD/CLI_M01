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

    // =========================================================
    // LIFECYCLE & CORE EXECUTION
    // =========================================================
    Process(int pid, std::string name, int totalLines = 0);
    void addCommand(std::shared_ptr<ICommand> command);
    void executeCurrentCommand();
    void moveToNextLine();

    // =========================================================
    // STATE ACCESSORS & GETTERS/SETTERS
    // =========================================================
    int getPID() const;
    std::string getName() const;
    ProcessState getState() const;
    void setState(ProcessState state);
    bool isFinished() const;
    SymbolTable& getSymbolTable();

    // =========================================================
    // DIAGNOSTIC METRICS
    // =========================================================
    int getLinesExecuted() const;
    int getTotalLines() const;
    int getSleepTicksRemaining() const { return sleepTicksRemaining; } // Kept inline (not in .cpp)
    int getAssignedCore() const;
    void setAssignedCore(int core);

    std::string getStartedAtString() const;
    std::string getLastUpdatedString() const;
    int getCurrentInstructionLine() const;
    int getCurrentFrameInstructionCount() const;
    void printExecutionLogs() const;

    // =========================================================
    // FLOW INTERCEPTION CONTROLS
    // =========================================================
    void pushLoopFrame(const std::vector<std::shared_ptr<ICommand>>& instructions, int repeats);
    void sleep(int ticks);
    void decrementSleepTicks();
    void touch();

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
    int assignedCore; // Fixed: Removed the duplicate declaration of this variable
    std::vector<std::string> logs;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point lastUpdatedAt;
};