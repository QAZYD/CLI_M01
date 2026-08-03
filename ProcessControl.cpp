#include "coreDependencies/ProcessControl.h" 
#include "memoryControl/MemoryManager.h"
#include "coreDependencies/CommandGenerator.h"
#include "ICommandChildren/SleepCommand.h" 
#include "ICommandChildren/ForCommand.h"   
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <algorithm>

// =========================================================
// LIFECYCLE & CORE EXECUTION
// =========================================================

Process::Process(int pid, std::string name, int totalLines, std::mt19937& gen, bool varPrint,
                 uint32_t memSize, uint32_t frameSize,
                 const std::vector<std::shared_ptr<ICommand>>& initialCommands)
    : pid(pid), 
      name(name), 
      currentState(READY), 
      isStackInitialized(false), 
      sleepTicksRemaining(0), 
      linesExecuted(0), 
      assignedCore(-1), 
      runStartTime("N/A"),
      totalInstructions(totalLines),
      memorySize(memSize),
      invalidAddress(0),
      errorTimestamp("")
{
    startedAt = std::chrono::system_clock::now();
    lastUpdatedAt = startedAt;

    // Allocate page table entries based on memory size and frame size
    uint32_t effectiveFrameSize = (frameSize > 0) ? frameSize : 16;
    uint32_t numPages = (memorySize + effectiveFrameSize - 1) / effectiveFrameSize;
    pageTable.resize(numPages);

    if (initialCommands.empty()) {
        commandList = CommandGenerator::generateProgram(totalLines, name, gen, varPrint);
    } else {
        commandList = initialCommands;
        totalInstructions = static_cast<int>(commandList.size());
    }
}

void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(command);
}

std::string Process::captureCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    std::time_t timeValue = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &timeValue);
#else
    localtime_r(&timeValue, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%m/%d/%Y %I:%M:%S %p");
    return stream.str();
}

void Process::touch() {
    lastUpdatedAt = std::chrono::system_clock::now();
}

// Legacy / Default Execution Call
void Process::executeCurrentCommand() {
    if (currentState == WAITING || currentState == FINISHED || currentState == MEMORY_VIOLATION) {
        return;
    }

    touch();
    
    if (!isStackInitialized) {
        if (!commandList.empty()) {
            executionStack.push_back({commandList, 0, 1});
        }
        isStackInitialized = true;
    }

    if (linesExecuted >= totalInstructions || executionStack.empty()) {
        currentState = FINISHED;
        return;
    }

    if (currentState == READY) {
        currentState = RUNNING;
    }

    auto& currentFrame = executionStack.back();
    if (currentFrame.pc >= 0 && currentFrame.pc < static_cast<int>(currentFrame.instructions.size())) {
        auto currentCmd = currentFrame.instructions[currentFrame.pc];
        linesExecuted++;

        std::string exactTime = captureCurrentTimestamp();
        int currentLine = getCurrentInstructionLine(); 
        int limitLines = getTotalLines();

        LogEntry entry = {
            currentCmd->toString(),
            exactTime,
            currentLine,
            limitLines,
            assignedCore
        };

        commandLogs.push_back(entry);
        executionHistory.push_back(entry);

        if (auto sleepCmd = std::dynamic_pointer_cast<SleepCommand>(currentCmd)) {
            this->sleep(sleepCmd->getTicks());
        } 
        else if (auto forCmd = std::dynamic_pointer_cast<ForCommand>(currentCmd)) {
            this->pushLoopFrame(forCmd->getInstructions(), forCmd->getRepeats());
        } 
        else {
            currentCmd->execute(symbolTable);
        }
    } else {
        currentState = FINISHED;
    }
}

// MemoryManager Integrated Execution Call
void Process::executeCurrentCommand(MemoryManager& memoryManager) {
    if (currentState == WAITING || currentState == FINISHED || currentState == MEMORY_VIOLATION) {
        return;
    }

    touch();

    if (!isStackInitialized) {
        if (!commandList.empty()) {
            executionStack.push_back({commandList, 0, 1});
        }
        isStackInitialized = true;
    }

    if (linesExecuted >= totalInstructions || executionStack.empty()) {
        currentState = FINISHED;
        return;
    }

    if (currentState == READY) {
        currentState = RUNNING;
    }

    auto& currentFrame = executionStack.back();
    if (currentFrame.pc >= 0 && currentFrame.pc < static_cast<int>(currentFrame.instructions.size())) {
        
        // =========================================================
        // DEMAND PAGING INTEGRATION
        // =========================================================
        uint32_t numPages = static_cast<uint32_t>(pageTable.size());
        if (numPages > 0) {
            // Map the current instruction line to a virtual page index
            uint32_t page_num = (linesExecuted) % numPages;

            // 1. Access the page (Triggers Page Fault -> Paged In counter + LRU Eviction if RAM full)
            int frame_id = memoryManager.access_page(*this, page_num);

            // 2. Mark frame dirty on access so eviction writes to backing store file
            // (Or call memoryManager.write_uint16(*this, virt_addr, val) if simulating memory writes)
            if (frame_id != -1) {
                uint16_t simulated_virt_addr = static_cast<uint16_t>((linesExecuted * 2) % memorySize);
                memoryManager.write_uint16(*this, simulated_virt_addr, 0x01);
            }
        }
        // =========================================================

        auto currentCmd = currentFrame.instructions[currentFrame.pc];
        linesExecuted++;

        std::string exactTime = captureCurrentTimestamp();
        int currentLine = getCurrentInstructionLine(); 
        int limitLines = getTotalLines();

        LogEntry entry = {
            currentCmd->toString(),
            exactTime,
            currentLine,
            limitLines,
            assignedCore
        };

        commandLogs.push_back(entry);
        executionHistory.push_back(entry);

        if (auto sleepCmd = std::dynamic_pointer_cast<SleepCommand>(currentCmd)) {
            this->sleep(sleepCmd->getTicks());
        } 
        else if (auto forCmd = std::dynamic_pointer_cast<ForCommand>(currentCmd)) {
            this->pushLoopFrame(forCmd->getInstructions(), forCmd->getRepeats());
        } 
        else {
            currentCmd->execute(*this, memoryManager);
        }
    } else {
        currentState = FINISHED;
    }
}

void Process::moveToNextLine() {
    // FIX 3: Do not advance program counter if process encountered a memory fault or is finished
    if (currentState == MEMORY_VIOLATION || currentState == FINISHED || executionStack.empty()) {
        if (currentState != MEMORY_VIOLATION) {
            currentState = FINISHED;
        }
        return;
    }

    executionStack.back().pc++;

    while (!executionStack.empty() && executionStack.back().pc >= static_cast<int>(executionStack.back().instructions.size())) {
        executionStack.back().repeatsLeft--;

        if (executionStack.back().repeatsLeft > 0) {
            executionStack.back().pc = 0; 
            break; 
        } else {
            executionStack.pop_back(); 
            if (!executionStack.empty()) {
                executionStack.back().pc++; 
            }
        }
    }

    if (executionStack.empty() && currentState != MEMORY_VIOLATION) {
        currentState = FINISHED;
    }
}

// =========================================================
// MEMORY & VIOLATION HANDLING (ADDED)
// =========================================================

void Process::triggerMemoryViolation(uint16_t faultAddr, const std::string& timestamp) {
    currentState = MEMORY_VIOLATION;
    invalidAddress = faultAddr;
    errorTimestamp = timestamp;
}

uint32_t Process::getMemorySize() const { return memorySize; }

std::vector<PageTableEntry>& Process::getPageTable() { return pageTable; }

const std::vector<PageTableEntry>& Process::getPageTable() const { return pageTable; }

uint16_t Process::getInvalidAddress() const { return invalidAddress; }

std::string Process::getErrorTimestamp() const { return errorTimestamp; }

// =========================================================
// STATE ACCESSORS & GETTERS/SETTERS
// =========================================================

int Process::getPID() const { return pid; }
std::string Process::getName() const { return name; }
Process::ProcessState Process::getState() const { return currentState; }

void Process::setState(ProcessState State) { 
    currentState = State; 
}

bool Process::isFinished() const {
    return currentState == FINISHED || currentState == MEMORY_VIOLATION || (isStackInitialized && executionStack.empty());
}

SymbolTable& Process::getSymbolTable() { return symbolTable; }

int Process::getAssignedCore() const { return assignedCore; }

void Process::setAssignedCore(int core) {
    assignedCore = core;
}

int Process::getLinesExecuted() const { return linesExecuted; }
int Process::getTotalLines() const { return totalInstructions; }

int Process::getCurrentInstructionLine() const {
    if (!isStackInitialized && !commandList.empty()) {
        return 1;
    }

    if (executionStack.empty()) {
        return 0;
    }

    const auto& currentFrame = executionStack.back();
    if (currentFrame.pc < 0 || currentFrame.pc >= static_cast<int>(currentFrame.instructions.size())) {
        return 0;
    }

    return currentFrame.pc + 1;
}

int Process::getCurrentFrameInstructionCount() const {
    if (!isStackInitialized || executionStack.empty()) {
        return static_cast<int>(commandList.size());
    }

    const auto& currentFrame = executionStack.back();
    return static_cast<int>(currentFrame.instructions.size());
}

std::string Process::getStartedAtString() const {
    std::time_t timeValue = std::chrono::system_clock::to_time_t(startedAt);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &timeValue);
#else
    localtime_r(&timeValue, &localTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%m/%d/%Y %I:%M:%S %p");
    return stream.str();
}

std::string Process::getLastUpdatedString() const {
    std::time_t timeValue = std::chrono::system_clock::to_time_t(lastUpdatedAt);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &timeValue);
#else
    localtime_r(&timeValue, &localTime);
#endif
    std::ostringstream stream;
    stream << std::put_time(&localTime, "%m/%d/%Y %I:%M:%S %p");
    return stream.str();
}

const std::vector<Process::LogEntry>& Process::getCommandLogs() const { 
    return commandLogs; 
}

void Process::clearCommandLogs() {
    commandLogs.clear();
}

void Process::printExecutionLogs() const {
    for (const auto& log : commandLogs) {
        std::cout << "[" << log.timestamp << "] Core " << log.coreId 
                  << " Line " << log.currentLine << "/" << log.totalLines 
                  << ": " << log.commandText << std::endl;
    }
}

// =========================================================
// FLOW INTERCEPTION CONTROLS
// =========================================================

void Process::pushLoopFrame(const std::vector<std::shared_ptr<ICommand>>& instructions, int repeats) {
    executionStack.push_back({instructions, -1, repeats});
}

void Process::sleep(int ticks) {
    sleepTicksRemaining = ticks;
    currentState = WAITING; 
}

void Process::decrementSleepTicks() {
    if (sleepTicksRemaining > 0) {
        sleepTicksRemaining--;
        if (sleepTicksRemaining == 0) {
            currentState = READY;
        }
    }
}

void Process::logLineExecution(int pcFrameIndex) {
    touch();
}
    
std::vector<std::string> Process::getInstructionStrings() const {
    std::vector<std::string> result;
    for (const auto& cmd : commandList) {
        if (cmd) result.push_back(cmd->toString());
    }
    return result;
}

const std::vector<Process::LogEntry>& Process::getExecutionHistory() const {
    return executionHistory;
}

void Process::setRunStartTime(const std::string& time) { runStartTime = time; }
std::string Process::getRunStartTime() const { return runStartTime; }