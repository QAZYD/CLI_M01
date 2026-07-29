#pragma once

#include <string>
#include <cstdint>

struct Config {
    int numCpu;
    std::string scheduler;

    uint32_t quantumCycles;
    uint32_t batchProcessFreq;

    uint32_t minIns;
    uint32_t maxIns;

    uint32_t delayPerExec;

    uint32_t maxoverallMem;
    uint32_t memperframe;
    uint32_t minmemperproc;
    uint32_t maxmemperproc;

    bool varPrint;
};