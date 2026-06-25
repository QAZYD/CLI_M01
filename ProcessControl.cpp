#include "coreDependencies/ProcessControl.h" 
#include "ICommandChildren/SleepCommand.h" // Needed for dynamic_pointer_cast checks
#include "IcommandChildren/ForCommand.h"   // Needed for dynamic_pointer_cast checks
#include <iostream>

Process::Process(int pid, std::string name)
    : pid(pid), name(name), currentState(READY), isStackInitialized(false), sleepTicksRemaining(0) {}

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

        //INTERCEPTION LAYER: Check if command is a control-flow wrapper
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