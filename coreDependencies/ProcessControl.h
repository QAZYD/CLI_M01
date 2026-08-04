#pragma once
#include <memory>
#include <random>
#include <vector>
#include <string>
#include <chrono>
#include "SymbolTable.h"
#include "ICommand.h"
#include "../memoryControl/memoryStructures.h"

// Forward declaration to prevent circular dependency loops with MemoryManager.h
class MemoryManager;

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
        int coreId;
    };

    struct ExecutionFrame {
        std::vector<std::shared_ptr<ICommand>> instructions;
        int pc = 0;
        int repeatsLeft = 1;
    };

    // =========================================================
    // LIFECYCLE & CORE EXECUTION
    // =========================================================
    // Constructor accepts optional memSize and frameSize (defaults to 4096 / 16)
    Process(int pid, std::string name, int totalLines, std::mt19937& gen, bool varPrint,
            uint32_t memSize = 4096, uint32_t frameSize = 16,
            const std::vector<std::shared_ptr<ICommand>>& initialCommands = {});

    void addCommand(std::shared_ptr<ICommand> command);
    void executeCurrentCommand();
    void executeCurrentCommand(MemoryManager& memoryManager);
    void moveToNextLine();
    std::string captureCurrentTimestamp() const;

    // =========================================================
    // STATE ACCESSORS & GETTERS/SETTERS
    // =========================================================
    int getPID() const;
    std::string getName() const;
    ProcessState getState() const;
    void setState(ProcessState State);
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
    void setMemorySize(uint32_t newSize);

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
    

    // =========================================================
    // MEMORY METRICS & ACCESSORS
    // =========================================================
    uint32_t getMemorySize() const;
    std::vector<PageTableEntry>& getPageTable();
    const std::vector<PageTableEntry>& getPageTable() const;

    std::string getErrorTimestamp() const;
    uint16_t getInvalidAddress() const;
    void triggerMemoryViolation(uint16_t address, const std::string& timestamp);

    bool hasMemoryViolation() const;
std::string getMemoryViolationTime() const;
uint16_t getMemoryViolationAddr() const;

private:
    int pid;
    
    std::string runStartTime;
    std::string name;
    ProcessState currentState;
    std::vector<LogEntry> commandLogs;
    std::vector<LogEntry> executionHistory;
    std::vector<std::string> printOutputs;
    
    std::vector<ExecutionFrame> executionStack;
    std::vector<std::shared_ptr<ICommand>> commandList; 
    SymbolTable symbolTable;
    bool isStackInitialized;

    int totalInstructions;
    int linesExecuted;
    int sleepTicksRemaining;
    int assignedCore;
    std::vector<std::string> logs;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point lastUpdatedAt;

    // Memory tracking
    uint32_t memorySize;                   
    std::vector<PageTableEntry> pageTable; 
    
    uint16_t invalidAddress = 0;           
    std::string errorTimestamp = "";       
};