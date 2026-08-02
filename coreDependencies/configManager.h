#pragma once
#include <string>
#include <cstdint>

struct Config {
    int numCpu = 12;
    std::string scheduler = "fcfs";
    int quantumCycles = 10;
    int batchProcessFreq = 150;
    int minIns = 10;
    int maxIns = 50;
    int delayPerExec = 20;
    bool varPrint = true;

    // --- MEMORY CONFIGURATION FIELDS (Defaults: 512, 256, 512, 512) ---
    uint32_t maxOverallMem = 512;    // Maximum memory available in bytes
    uint32_t memPerFrame = 256;      // Size of memory in bytes per frame / page
    uint32_t minMemPerProc = 512;    // Minimum memory required per process
    uint32_t maxMemPerProc = 512;    // Maximum memory required per process
};

class ConfigManager {
public:
    bool loadConfig(const std::string& filename);
    const Config& getConfig() const;

private:
    Config config;
};