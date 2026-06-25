// coreDependencies/Scheduler.h
#pragma once
#include <memory>
#include "ProcessControl.h" 

class Scheduler {
public:
    virtual ~Scheduler() = default;
    virtual void addProcess(std::shared_ptr<Process> process) = 0;
    virtual void run() = 0;
    virtual void stop() = 0;
};