    #include "coreDependencies/ProcessControl.h" 
    #include "CommandGenerator.h" // Handles our decoupled program generation
    #include "ICommandChildren/SleepCommand.h" // Kept for dynamic pointer checking
    #include "ICommandChildren/ForCommand.h"   // Kept for dynamic pointer checking

    #include <iostream>
    #include <iomanip>
    #include <sstream>
    #include <ctime>
    #include <chrono>
    #include <algorithm>

    // =========================================================
    // LIFECYCLE & CORE EXECUTION
    // =========================================================

    Process::Process(int pid, std::string name, int totalLines)
        : pid(pid), name(name), currentState(READY), isStackInitialized(false), 
        sleepTicksRemaining(0), linesExecuted(0), startedAt(std::chrono::system_clock::now()), 
        assignedCore(-1), totalInstructions(totalLines)
    {
        // Delegate the random layout construction to our specialized generator
        commandList = CommandGenerator::generateProgram(totalLines);
    }

    void Process::addCommand(std::shared_ptr<ICommand> command) {
        commandList.push_back(command);
    }

    void Process::executeCurrentCommand() {
        // 1. Lazy-initialize the call stack
        if (!isStackInitialized) {
            if (!commandList.empty()) {
                executionStack.push_back({commandList, 0, 1});
            }
            isStackInitialized = true;
        }

        // Stop immediately if we have reached or exceeded the totalLines budget
        if (linesExecuted >= totalInstructions) {
            currentState = FINISHED;
            return;
        }

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

    // =========================================================
    // COMPLEX DIAGNOSTIC METRICS
    // =========================================================

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