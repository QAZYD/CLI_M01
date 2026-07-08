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

    uint32_t maxOverallMem;
    uint32_t memPerFrame;
    uint32_t memPerProc;

    // FOR MCO2 - All memory ranges are [2^6,2^16] and the power of 2 format.
    /* 
    uint32_t minMemPerProc;
    uint32_t minMaxPerProc;
    */
    uint32_t delayPerExec;

    bool varPrint;
};