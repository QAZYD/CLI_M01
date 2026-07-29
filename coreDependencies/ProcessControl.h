#pragma once
#include <memory>
#include <random>
#include <vector>
#include <string>
#include <chrono>
#include "SymbolTable.h"
#include "ICommand.h"
#include  "../memoryControl/memoryStructures.h"
#include "../memoryControl/memoryManager.h"

class Process {
public:
    enum ProcessState {
        READY,
        RUNNING,
        WAITING,
        FINISHED,
        MEMORY_VIOLATION
    };

    struct LogEntry {
        std::string commandText;
        std::string timestamp;
        int currentLine;    
        int totalLines;
        int coreId;;
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
    Process(int pid, std::string name, int totalLines,  std::mt19937& gen, bool varPrint);
    void addCommand(std::shared_ptr<ICommand> command);
    void executeCurrentCommand();
    void moveToNextLine();
    std::string captureCurrentTimestamp() const;

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
    int getSleepTicksRemaining() const { return sleepTicksRemaining; } 
    int getAssignedCore() const;
    void setAssignedCore(int core);


    std::string getStartedAtString() const;
    std::string getLastUpdatedString() const;
    const std::vector<LogEntry>& getCommandLogs() const;
    int getCurrentInstructionLine() const;
    int getCurrentFrameInstructionCount() const;
    void printExecutionLogs() const;
    void clearCommandLogs();

    // =========================================================
    // FLOW INTERCEPTION CONTROLS
    // =========================================================
    void pushLoopFrame(const std::vector<std::shared_ptr<ICommand>>& instructions, int repeats);
    void sleep(int ticks);
    void decrementSleepTicks();
    void touch();
    void logLineExecution(int pcFrameIndex);
    std::vector<std::string> getInstructionStrings() const;
    const std::vector<LogEntry>& getExecutionHistory() const;
    void setRunStartTime(const std::string& time);
    std::string getRunStartTime() const;

    // --- ADDED MEMORY GETTERS & SETTERS ---
    uint32_t getMemorySize() const { return memorySize; }
    std::vector<PageTableEntry>& getPageTable() { return pageTable; }

    std::string getErrorTimestamp() const { return errorTimestamp; }
    uint16_t getInvalidAddress() const { return invalidAddress; }
    
    void triggerMemoryViolation(uint16_t address, const std::string& timestamp) {
        currentState = MEMORY_VIOLATION;
        invalidAddress = address;
        errorTimestamp = timestamp;
    }

    // Pass MemoryManager into execution cycle
    void executeCurrentCommand(MemoryManager& memoryManager);

private:
    // Identity & State
    int pid;
    std::string runStartTime;
    std::string name;
    ProcessState currentState;
    std::vector<LogEntry> commandLogs;
    std::vector<LogEntry> executionHistory;
    
    // Execution Stack
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

    // --- NEW MEMORY FIELDS ---
    uint32_t memorySize;                   // Allocated size (e.g. 256, 512, 1024)
    std::vector<PageTableEntry> pageTable; // Per-process Page Table
    
    uint16_t invalidAddress = 0;           // Address that caused violation
    std::string errorTimestamp = "";       // Timestamp when violation occurred[cite: 1]
};