#pragma once

class ICommand {
public:
    virtual ~ICommand() = default; // Clean up memory safely
    virtual void execute() = 0;    // Pure virtual function
};