#pragma once

class Process;     

class ICommand {
public:
    virtual ~ICommand() = default;
    //modified to give process to let command find symboltable and the process info
    virtual void execute(Process& process) = 0;
};