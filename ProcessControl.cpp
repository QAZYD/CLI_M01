#include "coreDependencies/ProcessControl.h" 
#include "ICommandChildren/SleepCommand.h" 
#include "IcommandChildren/ForCommand.h"   

// ---  CONCRETE ACTIONS ---
#include "ICommandChildren/DeclareCommand.h"
#include "ICommandChildren/AddCommand.h"
#include "ICommandChildren/SubtractCommand.h"
#include "ICommandChildren/PrintCommand.h"

#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <ctime>

// Simple placeholder class to fill up dummy execution processes safely
class DummyCommand : public ICommand {
public:
    void execute(SymbolTable& table) override {
        // Do nothing mock work cycle (represents standard SET, PRINT, ASSIGN statements)
    }
};

std::shared_ptr<ICommand> generateRandomCommand(std::mt19937& gen, int currentDepth, int& remainingInstructions, int& generatedInstructions) {
    std::uniform_int_distribution<int> typeDist(0, 5);  
    std::uniform_int_distribution<int> tickDist(1, 10);  
    std::uniform_int_distribution<int> repeatDist(2, 4);
    std::uniform_int_distribution<int> sizeDist(2, 3);   

    int choice = typeDist(gen);

    // Enforce the max loop nesting rule (up to 3 times)
    if (choice == 0 && currentDepth >= 3) {
    std::uniform_int_distribution<int> nonLoopDist(1, 5);
    choice = nonLoopDist(gen);
    }

    if (choice == 1) {
        remainingInstructions--;
        generatedInstructions++;

        uint8_t randomTicks = static_cast<uint8_t>(tickDist(gen));
        return std::make_shared<SleepCommand>(randomTicks);
    } 
    else if (choice == 0) {
    remainingInstructions--;  // FOR counts as one
    generatedInstructions++;


    int bodySize = std::min(
        sizeDist(gen),
        remainingInstructions
    );

    std::vector<std::shared_ptr<ICommand>> loopBody;

    for (int i = 0; i < bodySize; ++i) {
        loopBody.push_back(
            generateRandomCommand(
                gen,
                currentDepth + 1,
                remainingInstructions,
                generatedInstructions
            )
        );
    }

    return std::make_shared<ForCommand>(
        loopBody,
        repeatDist(gen)
    );
    }
     
    else {
        int actionChoice = choice;
        switch (actionChoice) {
            case 2: 
                // Double check your DeclareCommand constructor parameters too!
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<DeclareCommand>("mockVar", 0); 
            
            case 3: 
                // FIXED: Passes 3 strings to match (dest, op1, op2) -> e.g., mockVar = mockVar + 1
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<AddCommand>("mockVar", "mockVar", "1");
            
            case 4: 
                // FIXED: Passes 3 strings to match (dest, op1, op2) -> e.g., mockVar = mockVar - 1
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<SubtractCommand>("mockVar", "mockVar", "1");
            
            case 5: 
            default:
                // Double check your PrintCommand constructor parameters too!
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<PrintCommand>("mockVar");
        }
    }
}
Process::Process(int pid, std::string name, int totalLines)
    : pid(pid), name(name), currentState(READY), isStackInitialized(false),
      sleepTicksRemaining(0), linesExecuted(0), startedAt(std::chrono::system_clock::now()),
      lastUpdatedAt(std::chrono::system_clock::now()), assignedCore(-1), totalInstructions(totalLines)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    int remainingInstructions = totalLines;
    int generatedInstructions = 0;
    // Generate randomized instruction lines for the root script layout
    while (generatedInstructions < remainingInstructions)
{
    addCommand(
        generateRandomCommand(
            gen,
            1,
            remainingInstructions,
            generatedInstructions
        )
    );
}
}

void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(command);
    touch();
}

void Process::executeCurrentCommand() {
    // 1. Lazy-initialize the call stack
    if (!isStackInitialized) {
        if (!commandList.empty()) {
            executionStack.push_back({commandList, 0, 1});
        }
        isStackInitialized = true;
    }

    // --- ADDED LIMIT CHECK ---
    // Stop immediately if we have reached or exceeded the totalLines budget
    if (linesExecuted >= totalInstructions) {
        currentState = FINISHED;
        return;
    }
    // -------------------------

    if (executionStack.empty()) {
        currentState = FINISHED;
        return;
    }

    // Transition from READY to RUNNING
    if (currentState == READY) {
        currentState = RUNNING;
    }

    auto& currentFrame = executionStack.back();
    if (currentFrame.pc >= 0 && currentFrame.pc < static_cast<int>(currentFrame.instructions.size())) {
        auto currentCmd = currentFrame.instructions[currentFrame.pc];

        // Increment executed lines counter and log
        linesExecuted++;
        logs.push_back("Executed command line index: " + std::to_string(currentFrame.pc));
        touch();

        // INTERCEPTION LAYER
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
void Process::moveToNextLine() {
    touch();
    if (executionStack.empty()) {
        currentState = FINISHED;
        return;
    }

    // Advance the program counter for the top frame
    executionStack.back().pc++;

    // Evaluate and unwind finished frames
    while (!executionStack.empty() && executionStack.back().pc >= static_cast<int>(executionStack.back().instructions.size())) {
        executionStack.back().repeatsLeft--;

        if (executionStack.back().repeatsLeft > 0) {
            executionStack.back().pc = 0; // Reset loop back to its first instruction line
            break; 
        } else {
            executionStack.pop_back(); // This frame level is done. Pop it!
            if (!executionStack.empty()) {
                executionStack.back().pc++; // Move past the parent's FOR statement line
            }
        }
    }

    if (executionStack.empty()) {
        currentState = FINISHED;
    }
}

bool Process::isFinished() const {
    return currentState == FINISHED || (isStackInitialized && executionStack.empty());
}

int Process::getPID() const { return pid; }
Process::ProcessState Process::getState() const { return currentState; }
std::string Process::getName() const { return name; }
void Process::setState(ProcessState State) { currentState = State; touch(); }
SymbolTable& Process::getSymbolTable() { return symbolTable; }

// =========================================================
// NEW DIAGNOSTIC READOUT METRICS
// =========================================================

int Process::getLinesExecuted() const { 
    return linesExecuted; 
}

int Process::getTotalLines() const {
    return totalInstructions;
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
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
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
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

void Process::touch() {
    lastUpdatedAt = std::chrono::system_clock::now();
}

int Process::getAssignedCore() const {
    return assignedCore;
}

void Process::setAssignedCore(int core) {
    assignedCore = core;
    touch();
}

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

void Process::printExecutionLogs() const {
    if (logs.empty()) {
        std::cout << "  (No execution logs recorded yet for this process)\n";
        return;
    }
    for (const auto& logEntry : logs) {
        std::cout << "  [Log] " << logEntry << "\n";
    }
}

// =========================================================
// FLOW CONTROLS
// =========================================================

void Process::pushLoopFrame(const std::vector<std::shared_ptr<ICommand>>& instructions, int repeats) {
    // pc is initialized to -1 because moveToNextLine() runs right after execute,
    // which increments it back up to 0 for the subsequent cycle tick.
    executionStack.push_back({instructions, -1, repeats});
    touch();
}

void Process::sleep(int ticks) {
    sleepTicksRemaining = ticks;
    currentState = WAITING; 
    touch();
}

void Process::decrementSleepTicks() {
    if (sleepTicksRemaining > 0) {
        sleepTicksRemaining--;
        if (sleepTicksRemaining == 0) {
            currentState = READY;
        }
    }
}