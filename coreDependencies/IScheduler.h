#ifndef ISCHEDULER_H
#define ISCHEDULER_H

#include <memory>
#include "ProcessControl.h"

class IScheduler {
public:
    virtual ~IScheduler() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void pushProcess(std::shared_ptr<Process> process) = 0;
    virtual int getCPUCycles() const = 0;
    virtual int getTotalCores() const = 0;

    // Added CPU Tick Metrics for vmstat
    virtual int getActiveCPUTicks() const = 0;
    virtual int getIdleCPUTicks() const = 0;
};

#endif