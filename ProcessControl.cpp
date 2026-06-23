#include "ProcessControl.h" 

// Constructor: Initialize identifier fields, set Program Counter to 0, and default to READY
Process::Process(int pid, std::string name)
    : pid(pid), name(name), currentState(READY), commandCounter(0) {}

// Appends an instruction pointer to the end of this process's program memory
void Process::addCommand(std::shared_ptr<ICommand> command) {
    commandList.push_back(command);
}

// Executes the single command located at the current commandCounter index
void Process::executeCurrentCommand() {
    // Boundary check: ensure we haven't read past the end of the program script
    if (commandCounter < commandList.size()) {
        // If the process was READY, it transitions to RUNNING when it gets CPU time
        if (currentState == READY) {
            currentState = RUNNING;
        }
        
        // Pass this individual process's private symbol table sandbox into the instruction
        commandList[commandCounter]->execute(symbolTable);
    } else {
        currentState = FINISHED;
    }
}

// Advances the program counter by one instruction line
void Process::moveToNextLine() {
    commandCounter++;
    
    // Automatically flag the process as FINISHED if it runs out of instructions
    if (commandCounter >= commandList.size()) {
        currentState = FINISHED;
    }
}

// Returns true if the process state machine has marked its context as FINISHED
bool Process::isFinished() const {
    return currentState == FINISHED;
}

// Getter: Fetch Process Identification Number
int Process::getPID() const {
    return pid;
}

// Getter: Fetch current life-cycle state enumeration
Process::ProcessState Process::getState() const {
    return currentState;
}

// Getter: Fetch human-readable process string name
std::string Process::getName() const {
    return name;
}

// Setter: Crucial method allowing your future OS Scheduler to force transition states 
// (e.g., manually pausing to WAITING or moving back to READY)
void Process::setState(ProcessState State) {
    currentState = State;
}

// Memory Reference: Exposes the isolated sandbox storage backend directly
SymbolTable& Process::getSymbolTable() {
    return symbolTable;
}