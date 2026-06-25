#include "coreDependencies/ProcessControl.h" 
#include "ICommandChildren/SleepCommand.h" 
#include "IcommandChildren/ForCommand.h"   

// ---  CONCRETE ACTIONS ---
#include "ICommandChildren/DeclareCommand.h"
#include "ICommandChildren/AddCommand.h"
#include "ICommandChildren/SubtractCommand.h"
#include "ICommandChildren/PrintCommand.h"

#include <iostream>
#include <random>

// Simple placeholder class to fill up dummy execution processes safely
class DummyCommand : public ICommand {
public:
    void execute(SymbolTable& table) override {
        // Do nothing mock work cycle (represents standard SET, PRINT, ASSIGN statements)
    }
};

std::shared_ptr<ICommand> generateRandomCommand(std::mt19937& gen, int currentDepth) {
    std::uniform_int_distribution<int> typeDist(0, 2);   // 0: Action, 1: Sleep, 2: For Loop
    std::uniform_int_distribution<int> actionDist(0, 3); // 0: Declare, 1: Add, 2: Subtract, 3: Print

    std::uniform_int_distribution<int> tickDist(1, 10);  
    std::uniform_int_distribution<int> repeatDist(2, 4); 
    std::uniform_int_distribution<int> sizeDist(2, 3);   

    int choice = typeDist(gen);

    // Enforce the max loop nesting rule (up to 3 times)
    if (choice == 2 && currentDepth >= 3) {
        choice = typeDist(gen) % 2; 
    }

    if (choice == 1) {
        uint8_t randomTicks = static_cast<uint8_t>(tickDist(gen));
        return std::make_shared<SleepCommand>(randomTicks);
    } 
    else if (choice == 2) {
        int bodySize = sizeDist(gen);
        std::vector<std::shared_ptr<ICommand>> loopBody;
        
        for (int i = 0; i < bodySize; ++i) {
            loopBody.push_back(generateRandomCommand(gen, currentDepth + 1));
        }
        return std::make_shared<ForCommand>(loopBody, repeatDist(gen));
    } 
    else {
        int actionChoice = actionDist(gen);
        switch (actionChoice) {
            case 0: 
                // Double check your DeclareCommand constructor parameters too!
                return std::make_shared<DeclareCommand>("mockVar", 0); 
            
            case 1: 
                // FIXED: Passes 3 strings to match (dest, op1, op2) -> e.g., mockVar = mockVar + 1
                return std::make_shared<AddCommand>("mockVar", "mockVar", "1");
            
            case 2: 
                // FIXED: Passes 3 strings to match (dest, op1, op2) -> e.g., mockVar = mockVar - 1
                return std::make_shared<SubtractCommand>("mockVar", "mockVar", "1");
            
            case 3: 
            default:
                // Double check your PrintCommand constructor parameters too!
                return std::make_shared<PrintCommand>("mockVar");
        }
    }
}
Process::Process(int pid, std::string name, int totalLines)
    : pid(pid), name(name), currentState(READY), isStackInitialized(false), 
      sleepTicksRemaining(0), linesExecuted(0) 
{
    std::random_device rd;
    std::mt19937 gen(rd());

    // Generate randomized instruction lines for the root script layout
    for (int i = 0; i < totalLines; ++i) {
        addCommand(generateRandomCommand(gen, 1)); // Top-level starts at Depth 1
    }
}

void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(command);
}

void Process::executeCurrentCommand() {
    // Lazy-initialize the call stack with our main command list on first execution
    if (!isStackInitialized) {
        if (!commandList.empty()) {
            executionStack.push_back({commandList, 0, 1});
        }
        isStackInitialized = true;
    }

    if (executionStack.empty()) {
        currentState = FINISHED;
        return;
    }

    // Transition from READY to RUNNING when matched with a CPU core
    if (currentState == READY) {
        currentState = RUNNING;
    }

    auto& currentFrame = executionStack.back();
    if (currentFrame.pc >= 0 && currentFrame.pc < static_cast<int>(currentFrame.instructions.size())) {
        auto currentCmd = currentFrame.instructions[currentFrame.pc];

        // Increment executed lines counter and log this cycle activity step
        linesExecuted++;
        logs.push_back("Executed command line index: " + std::to_string(currentFrame.pc));

        // INTERCEPTION LAYER: Check if command is a control-flow wrapper
        if (auto sleepCmd = std::dynamic_pointer_cast<SleepCommand>(currentCmd)) {
            this->sleep(sleepCmd->getTicks());
        } 
        else if (auto forCmd = std::dynamic_pointer_cast<ForCommand>(currentCmd)) {
            this->pushLoopFrame(forCmd->getInstructions(), forCmd->getRepeats());
        } 
        else {
            // Standard commands (SET, PRINT, ASSIGN) execute normally via your old system
            currentCmd->execute(symbolTable);
        }
    } else {
        currentState = FINISHED;
    }
}

void Process::moveToNextLine() {
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
void Process::setState(ProcessState State) { currentState = State; }
SymbolTable& Process::getSymbolTable() { return symbolTable; }

// =========================================================
// NEW DIAGNOSTIC READOUT METRICS
// =========================================================

int Process::getLinesExecuted() const { 
    return linesExecuted; 
}

int Process::getTotalLines() const { 
    return static_cast<int>(commandList.size()); 
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