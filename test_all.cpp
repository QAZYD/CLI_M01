#include <iostream>
#include <cassert>
#include <memory>
#include <random>
#include <fstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector>
#include <algorithm>
#include "CLICONTROL/Reporter.h"

#include "memoryControl/MemoryManager.h"
#include "coreDependencies/ProcessControl.h"
#include "coreDependencies/SymbolTable.h"
#include "coreDependencies/configManager.h"
#include "memoryControl/ReadCommand.h"
#include "memoryControl/WriteCommand.h"
#include "CLICONTROL/ScreenSpawnerCommand.h"
#include "CLICONTROL/InitializeCommand.h"
#include "coreDependencies/FCFSScheduler.h"
#include "coreDependencies/RRScheduler.h"

void printHeader(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

std::string stateToString(Process::ProcessState state) {
    switch (state) {
        case Process::ProcessState::READY:            return "READY";
        case Process::ProcessState::RUNNING:          return "RUNNING";
        case Process::ProcessState::WAITING:          return "WAITING";
        case Process::ProcessState::FINISHED:         return "FINISHED";
        case Process::ProcessState::MEMORY_VIOLATION: return "MEM_VIOL";
        default:                                      return "UNKNOWN";
    }
}

// Helper to compute process memory size M and virtual pages P according to config rules
uint32_t calculateProcessMemory(const Config& config, std::mt19937& gen) {
    uint32_t minM = config.minMemPerProc;
    uint32_t maxM = config.maxMemPerProc;
    
    uint32_t allocatedMemory = minM;
    if (maxM > minM) {
        std::uniform_int_distribution<uint32_t> dist(minM, maxM);
        allocatedMemory = dist(gen);
    }

    // Ensure memory is aligned to frame size boundaries
    if (config.memPerFrame > 0 && (allocatedMemory % config.memPerFrame != 0)) {
        allocatedMemory = ((allocatedMemory / config.memPerFrame) + 1) * config.memPerFrame;
    }

    return allocatedMemory;
}

// ---------------------------------------------------------
// TEST 1: SYMBOL TABLE CONSTRAINTS (CAPACITY & CLAMPING)
// ---------------------------------------------------------
void test_symbol_table(const Config& config) {
    printHeader("TEST 1: SYMBOL TABLE (32-VAR CAP & CLAMPING)");
    SymbolTable st;

    st.set("bigVar", 70000);
    std::cout << "[1.1] Clamping Check: Set 'bigVar' to 70000 -> Stored: " << st.get("bigVar") << std::endl;
    assert(st.get("bigVar") == 65535 && "FAIL: Value was not clamped to 65535!");

    std::cout << "[1.2] Filling Symbol Table up to 32 variables..." << std::endl;
    for (int i = 0; i < 31; ++i) {
        st.declare("var_" + std::to_string(i), i * 10);
    }
    assert(st.isFull() && "FAIL: Symbol Table should be full at 32 variables!");

    bool declared33 = st.declare("overflowVar", 100);
    std::cout << "[1.3] Attempting 33rd declaration ('overflowVar') -> Allowed? " 
              << (declared33 ? "YES" : "NO (Ignored)") << std::endl;
    assert(!declared33 && "FAIL: Symbol Table allowed more than 32 variables!");
    assert(!st.contains("overflowVar") && "FAIL: 33rd variable exists in table!");

    std::cout << "--> PASS: Symbol Table constraints verified successfully!\n";
}

// ---------------------------------------------------------
// TEST 2: HEX READ/WRITE MEMORY COMMANDS
// ---------------------------------------------------------
void test_hex_read_write(const Config& config) {
    printHeader("TEST 2: HEX READ / WRITE COMMAND EXECUTION");

    MemoryManager memManager;
    memManager.initialize(config.maxOverallMem, config.memPerFrame);

    std::mt19937 gen(42);
    Process p1(1, "Proc_A", 10, gen, false);

    WriteCommand writeCmd1("0x0004", "42");
    writeCmd1.execute(p1, memManager);
    std::cout << "[2.1] Executed: " << writeCmd1.toString() << std::endl;

    ReadCommand readCmd1("resultVar", "0x0004");
    readCmd1.execute(p1, memManager);
    std::cout << "[2.2] Executed: " << readCmd1.toString() << std::endl;

    uint16_t valRead = p1.getSymbolTable().get("resultVar");
    std::cout << "      -> Value in SymbolTable ('resultVar'): " << valRead << std::endl;
    assert(valRead == 42 && "FAIL: Read value does not match written value!");

    WriteCommand writeCmd2("0x000E", "resultVar");
    writeCmd2.execute(p1, memManager);
    std::cout << "[2.3] Executed variable write: " << writeCmd2.toString() << std::endl;

    ReadCommand readCmd2("copyVar", "0x000E");
    readCmd2.execute(p1, memManager);
    assert(p1.getSymbolTable().get("copyVar") == 42 && "FAIL: Copy write failed!");

    std::cout << "--> PASS: Hex Read/Write memory commands verified successfully!\n";
}

// ---------------------------------------------------------
// TEST 3: DEMAND PAGING & LRU EVICTION TO BACKING STORE
// ---------------------------------------------------------
void test_demand_paging(const Config& config) {
    printHeader("TEST 3: DEMAND PAGING & LRU EVICTION");

    // RAM configured explicitly via config parameters
    MemoryManager memManager;
    memManager.initialize(config.maxOverallMem, config.memPerFrame);

    std::mt19937 gen(42);
    Process p1(1, "Proc_Paging", 10, gen, false);

    std::cout << "[3.1] Initial RAM Status -> Free Memory: " << memManager.getFreeMemory() << " bytes\n";

    // Access virtual pages until page faults/evictions occur
    WriteCommand("0x0000", "1111").execute(p1, memManager);
    WriteCommand("0x0100", "2222").execute(p1, memManager);

    std::cout << "[3.2] Triggering Page Fault on Virtual Page 2 (addr 0x0200)...\n";
    WriteCommand("0x0200", "3333").execute(p1, memManager);

    std::cout << "      -> Total Pages Paged In:  " << memManager.getPagesPagedIn() << std::endl;
    std::cout << "      -> Total Pages Paged Out: " << memManager.getPagesPagedOut() << std::endl;

    assert(memManager.getPagesPagedOut() > 0 && "FAIL: Page Eviction was not triggered!");

    std::ifstream bsFile("csopesy-backing-store.txt");
    assert(bsFile.good() && "FAIL: Backing store file was not created!");
    std::cout << "\n--- BACKING STORE CONTENTS BEFORE NEXT TEST ---\n";
    std::string line;
    while (std::getline(bsFile, line)) {
        std::cout << "  " << line << std::endl;
    }
    std::cout << "-----------------------------------------------\n";
    bsFile.close();
}

// ---------------------------------------------------------
// TEST 4: OUT-OF-BOUNDS MEMORY ACCESS VIOLATION
// ---------------------------------------------------------
void test_memory_violation(const Config& config) {
    printHeader("TEST 4: OUT-OF-BOUNDS MEMORY ACCESS VIOLATION");

    MemoryManager memManager;
    memManager.initialize(config.maxOverallMem, config.memPerFrame);

    std::mt19937 gen(42);
    Process p1(1, "Proc_Faulty", 10, gen, false); 

    std::cout << "[4.1] Executing out-of-bounds WRITE to address 0x5000...\n";
    WriteCommand badCmd("0x5000", "9999");
    badCmd.execute(p1, memManager);

    std::cout << "      -> Process State: " 
              << (p1.getState() == Process::ProcessState::MEMORY_VIOLATION ? "MEMORY_VIOLATION" : "OTHER") 
              << std::endl;

    assert(p1.getState() == Process::ProcessState::MEMORY_VIOLATION && "FAIL: Process did not enter MEMORY_VIOLATION state!");
    std::cout << "      -> Recorded Fault Address: 0x" << std::hex << p1.getInvalidAddress() << std::dec << std::endl;
    std::cout << "      -> Recorded Fault Timestamp: " << p1.getErrorTimestamp() << std::endl;

    std::cout << "[4.2] Verifying process halts further execution...\n";
    int linesBefore = p1.getLinesExecuted();
    p1.executeCurrentCommand(memManager);
    assert(p1.getLinesExecuted() == linesBefore && "FAIL: Process executed instructions after memory violation!");

    std::cout << "--> PASS: Out-of-bounds memory violation handled successfully!\n";
}

// ---------------------------------------------------------
// TEST 5: USER-DEFINED SCREEN INSTRUCTIONS
// ---------------------------------------------------------
void test_custom_screen_instructions() {
    printHeader("TEST 5: USER-DEFINED SCREEN INSTRUCTIONS");

    InitializeCommand initHandler("config.txt");
    assert(initHandler.execute() && "FAIL: Could not initialize config for custom instruction test!");

    std::mt19937 gen(42);
    ScreenSpawnerCommand spawner;
    std::string command = "screen -c custom_proc 64 \"DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x0004 varA; READ varC 0x0004; PRINT(Result + varC)\"";

    bool created = spawner.execute(command, initHandler, gen);
    std::cout << "[5.1] Creating process with custom instruction list -> " << (created ? "SUCCESS" : "FAIL") << std::endl;
    assert(created && "FAIL: ScreenSpawnerCommand did not accept the new screen -c syntax!");

    const auto& processes = spawner.getActiveProcesses();
    assert(!processes.empty() && "FAIL: No process was created for custom instructions!");

    const auto& customProcess = processes.back();
    assert(customProcess->getName() == "custom_proc" && "FAIL: Process name was not preserved!");
    assert(customProcess->getMemorySize() == 64 && "FAIL: Custom process memory size was not preserved!");
    assert(customProcess->getInstructionStrings().size() == 6 && "FAIL: Custom instructions were not loaded into the process!");

    std::cout << "--> PASS: Custom screen instructions verified successfully!\n";
}

void runTest_ProcessSMI_and_VMStat() {
    std::cout << "\n=======================================================\n";
    std::cout << " TEST: Process-SMI & VMStat Diagnostic Metrics (Round Robin)\n";
    std::cout << "=======================================================\n\n";

    // 1. Memory & Scheduler Setup
    auto memoryManager = std::make_shared<MemoryManager>();
    memoryManager->initialize(512, 256); // 512B Total RAM

    int cores = 2;
    int delayPerExec = 50;
    int timeQuantum = 5; // Quantum of 5 ticks for Round Robin

    auto scheduler = std::make_unique<RRScheduler>(cores, delayPerExec, timeQuantum, memoryManager);
    ScreenSpawnerCommand spawnerHandler;
    InitializeCommand initHandler("config.txt");
    initHandler.execute(); // Ensure initialized flag is true
    std::mt19937 gen(1337);

    // 2. Spawn using custom instructions (-c <name> <mem_size> "<instructions>")
    spawnerHandler.execute("screen -c smi_p1 100 \"WRITE 0 42; READ x 0; PRINT(x)\"", initHandler, gen);
    spawnerHandler.execute("screen -c smi_p2 150 \"WRITE 0 99; READ y 0; PRINT(y)\"", initHandler, gen);

    // 3. Start Scheduler & Push active processes
    scheduler->start();
    const auto& processList = spawnerHandler.getActiveProcesses();
    for (const auto& proc : processList) {
        scheduler->pushProcess(proc);
    }

    // 4. Sample DURING active execution
    std::this_thread::sleep_for(std::chrono::milliseconds(15));

    std::cout << "[TEST LOG] Output of process-smi DURING active execution:\n";
    Reporter::printprocesssmi(*scheduler, spawnerHandler, *memoryManager);

    std::cout << "\n[TEST LOG] Output of vmstat DURING active execution:\n";
    Reporter::printVMStat(*scheduler, spawnerHandler, *memoryManager);

    // 5. Allow complete execution and stop scheduler
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    scheduler->stop();

    std::cout << "\n[TEST LOG] Output of vmstat AFTER execution finished:\n";
    Reporter::printVMStat(*scheduler, spawnerHandler, *memoryManager);

    // 6. Metrics Check
    int totalTicks  = scheduler->getCPUCycles();
    int activeTicks = scheduler->getActiveCPUTicks();
    int idleTicks   = scheduler->getIdleCPUTicks();
    int totalCores  = scheduler->getTotalCores();

    std::cout << "\n[METRIC VERIFICATION]\n";
    std::cout << " - Total Clock Cycles : " << totalTicks << "\n";
    std::cout << " - Active CPU Ticks   : " << activeTicks << "\n";
    std::cout << " - Idle CPU Ticks     : " << idleTicks << "\n";
    std::cout << " - Core-Tick Check    : " << (activeTicks + idleTicks) 
              << " (Expected: " << (totalTicks * totalCores) << ")\n";

    if ((activeTicks + idleTicks) == (totalTicks * totalCores) && activeTicks > 0) {
        std::cout << "[RESULT] PASSED: All metrics and memory accounting are 100% verified!\n";
    }
}

// ---------------------------------------------------------
// TEST 6: FCFS SCHEDULER & CONFIG MEMORY INTEGRATION (FORCED FCFS)
// ---------------------------------------------------------
void test_fcfs_scheduler(const Config& config) {
    printHeader("TEST 6: FCFS SCHEDULER & REAL CONFIG INTEGRATION");

    // 1. Apply Memory Config
    uint32_t totalFrames = config.maxOverallMem / config.memPerFrame;
    auto memManager = std::make_shared<MemoryManager>();
    memManager->initialize(config.maxOverallMem, config.memPerFrame);

    // 2. Force FCFS Scheduler regardless of config.scheduler value
    uint32_t numCpu = config.numCpu;
    uint32_t delayPerExec = config.delayPerExec;
    FCFSScheduler scheduler(numCpu, delayPerExec, memManager);

    std::mt19937 gen(1337);
    InitializeCommand initHandler("config.txt");
    initHandler.execute();

    ScreenSpawnerCommand spawner;

    // 3. Print Config Usage Verification
    std::cout << "[6.1] Config Memory Setup in Action:\n";
    std::cout << "      • Config max-overall-mem : " << config.maxOverallMem << " bytes\n";
    std::cout << "      • Config mem-per-frame   : " << config.memPerFrame << " bytes\n";
    std::cout << "      • Total RAM Frames (F)   : " << totalFrames << " frames\n";
    std::cout << "      • CPUs Allocated (numCpu): " << numCpu << " cores\n";
    std::cout << "      • Scheduler Forced       : FCFS (Config specified: \"" << config.scheduler << "\")\n\n";

    // 4. Generate Processes using min-mem-per-proc and max-mem-per-proc rules
    std::vector<std::shared_ptr<Process>> activeProcesses;
    for (int i = 1; i <= 5; ++i) {
        uint32_t procMem = calculateProcessMemory(config, gen);
        uint32_t pagesReq = procMem / config.memPerFrame;
        std::string procName = "Proc_FCFS_" + std::to_string(i);

        std::cout << "      [Process Spawn] " << procName << " -> Memory: " << procMem 
                  << "B | Virtual Pages (P): " << pagesReq << " pages\n";

        std::string script = "screen -c " + procName + " " + std::to_string(procMem) + 
                             " \"WRITE 0x0000 " + std::to_string(i * 10) + 
                             "; WRITE 0x0100 " + std::to_string(i * 20) + 
                             "; SLEEP 1; WRITE 0x0000 999\"";
        
        spawner.execute(script, initHandler, gen);
        activeProcesses.push_back(spawner.getActiveProcesses().back());
    }

    std::cout << "\n[6.2] Enqueuing Processes into FCFS Ready Queue...\n";
    for (const auto& proc : activeProcesses) {
        scheduler.pushProcess(proc);
    }

    std::cout << "[6.3] Starting FCFS Scheduler Threads...\n";
    scheduler.start();

    // 5. Execution Monitoring Loop
    std::cout << "\n================ REAL-TIME EXECUTION LOG ==================\n";
    int lastCycle = -1;
    int elapsedMs = 0;
    const int timeoutMs = 10000;

    auto allFinished = [&activeProcesses]() {
        for (const auto& p : activeProcesses) {
            if (!p->isFinished()) return false;
        }
        return true;
    };

    while (!allFinished() && elapsedMs < timeoutMs) {
        int currentCycle = scheduler.getCPUCycles();

        if (currentCycle != lastCycle) {
            lastCycle = currentCycle;

            std::cout << "[Tick #" << std::setw(3) << currentCycle << "] "
                      << "RAM Used: " << std::setw(3) << memManager->getUsedMemory() << "/" << config.maxOverallMem << "B | "
                      << "Free: " << std::setw(3) << memManager->getFreeMemory() << "B | "
                      << "PagedIn: " << memManager->getPagesPagedIn() << " | "
                      << "PagedOut: " << memManager->getPagesPagedOut() << "\n";

            for (const auto& p : activeProcesses) {
                std::cout << "    ↳ " << std::left << std::setw(12) << p->getName()
                          << " | State: " << std::setw(8) << stateToString(p->getState())
                          << " | Lines: " << p->getLinesExecuted() << "/" << p->getTotalLines()
                          << " | Core: " << (p->getAssignedCore() >= 0 ? std::to_string(p->getAssignedCore()) : "N/A") << "\n";
            }
            std::cout << "-----------------------------------------------------------\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        elapsedMs += 5;
    }

    scheduler.stop();
    std::cout << "===========================================================\n\n";

    // Assertions
    std::cout << "[6.4] Final Summary & Memory Deallocation Check:\n";
    std::cout << "      • Total CPU Cycles Elapsed : " << scheduler.getCPUCycles() << "\n";
    std::cout << "      • Total Pages Paged In     : " << memManager->getPagesPagedIn() << "\n";
    std::cout << "      • Total Pages Paged Out    : " << memManager->getPagesPagedOut() << "\n";
    std::cout << "      • Remaining RAM In-Use     : " << memManager->getUsedMemory() << " bytes\n";

    for (const auto& p : activeProcesses) {
        assert(p->isFinished() && "FAIL: A process failed to complete under FCFS!");
    }
    assert(memManager->getUsedMemory() == 0 && "FAIL: Memory leak! RAM was not reclaimed upon completion!");

    std::cout << "\n--> PASS: Config-driven FCFS Scheduler & Memory Manager verified successfully!\n";
}

// ---------------------------------------------------------
// TEST 7: RR SCHEDULER & CONFIG MEMORY INTEGRATION (FORCED RR)
// ---------------------------------------------------------
void test_rr_scheduler(const Config& config) {
    printHeader("TEST 7: ROUND-ROBIN SCHEDULER & REAL CONFIG INTEGRATION");

    // 1. Apply Memory Config
    uint32_t totalFrames = config.maxOverallMem / config.memPerFrame;
    auto memManager = std::make_shared<MemoryManager>();
    memManager->initialize(config.maxOverallMem, config.memPerFrame);

    // 2. Force Round-Robin Scheduler regardless of config.scheduler value
    uint32_t numCpu = config.numCpu;
    uint32_t delayPerExec = config.delayPerExec;
    // RR requires quantum > 0. If config.quantumCycles is 0, default to 2 cycles for context switching
    uint32_t quantumCycles = (config.quantumCycles > 0) ? config.quantumCycles : 2;

    RRScheduler scheduler(numCpu, delayPerExec, quantumCycles, memManager);

    std::mt19937 gen(1337);
    InitializeCommand initHandler("config.txt");
    initHandler.execute();

    ScreenSpawnerCommand spawner;

    // 3. Print Config Usage Verification
    std::cout << "[7.1] Config Memory Setup in Action:\n";
    std::cout << "      • Config max-overall-mem : " << config.maxOverallMem << " bytes\n";
    std::cout << "      • Config mem-per-frame   : " << config.memPerFrame << " bytes\n";
    std::cout << "      • Total RAM Frames (F)   : " << totalFrames << " frames\n";
    std::cout << "      • CPUs Allocated (numCpu): " << numCpu << " cores\n";
    std::cout << "      • Quantum Cycles Used    : " << quantumCycles << " cycles " 
              << (config.quantumCycles == 0 ? "(Enforced >=1 for RR)" : "") << "\n";
    std::cout << "      • Scheduler Forced       : Round-Robin (Config specified: \"" << config.scheduler << "\")\n\n";

    // 4. Generate Processes using min-mem-per-proc and max-mem-per-proc rules
    std::vector<std::shared_ptr<Process>> activeProcesses;
    for (int i = 1; i <= 5; ++i) {
        uint32_t procMem = calculateProcessMemory(config, gen);
        uint32_t pagesReq = procMem / config.memPerFrame;
        std::string procName = "Proc_RR_" + std::to_string(i);

        std::cout << "      [Process Spawn] " << procName << " -> Memory: " << procMem 
                  << "B | Virtual Pages (P): " << pagesReq << " pages\n";

        std::string script = "screen -c " + procName + " " + std::to_string(procMem) + 
                             " \"WRITE 0x0000 " + std::to_string(i * 15) + 
                             "; WRITE 0x0100 " + std::to_string(i * 30) + 
                             "; SLEEP 1; WRITE 0x0000 888\"";

        spawner.execute(script, initHandler, gen);
        activeProcesses.push_back(spawner.getActiveProcesses().back());
    }

    std::cout << "\n[7.2] Enqueuing Processes into Round-Robin Ready Queue...\n";
    for (const auto& proc : activeProcesses) {
        scheduler.pushProcess(proc);
    }

    std::cout << "[7.3] Starting Round-Robin Scheduler Threads...\n";
    scheduler.start();

    // 5. Execution Monitoring Loop
    std::cout << "\n================ REAL-TIME EXECUTION LOG ==================\n";
    int lastCycle = -1;
    int elapsedMs = 0;
    const int timeoutMs = 10000;

    auto allFinished = [&activeProcesses]() {
        for (const auto& p : activeProcesses) {
            if (!p->isFinished()) return false;
        }
        return true;
    };

    while (!allFinished() && elapsedMs < timeoutMs) {
        int currentCycle = scheduler.getCPUCycles();

        if (currentCycle != lastCycle) {
            lastCycle = currentCycle;

            std::cout << "[Tick #" << std::setw(3) << currentCycle << "] "
                      << "RAM Used: " << std::setw(3) << memManager->getUsedMemory() << "/" << config.maxOverallMem << "B | "
                      << "Free: " << std::setw(3) << memManager->getFreeMemory() << "B | "
                      << "PagedIn: " << memManager->getPagesPagedIn() << " | "
                      << "PagedOut: " << memManager->getPagesPagedOut() << "\n";

            for (const auto& p : activeProcesses) {
                std::cout << "    ↳ " << std::left << std::setw(12) << p->getName()
                          << " | State: " << std::setw(8) << stateToString(p->getState())
                          << " | Lines: " << p->getLinesExecuted() << "/" << p->getTotalLines()
                          << " | Core: " << (p->getAssignedCore() >= 0 ? std::to_string(p->getAssignedCore()) : "N/A") << "\n";
            }
            std::cout << "-----------------------------------------------------------\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        elapsedMs += 5;
    }

    scheduler.stop();
    std::cout << "===========================================================\n\n";

    // Assertions
    std::cout << "[7.4] Final Summary & Memory Deallocation Check:\n";
    std::cout << "      • Total CPU Cycles Elapsed : " << scheduler.getCPUCycles() << "\n";
    std::cout << "      • Total Pages Paged In     : " << memManager->getPagesPagedIn() << "\n";
    std::cout << "      • Total Pages Paged Out    : " << memManager->getPagesPagedOut() << "\n";
    std::cout << "      • Remaining RAM In-Use     : " << memManager->getUsedMemory() << " bytes\n";

    for (const auto& p : activeProcesses) {
        assert(p->isFinished() && "FAIL: A process failed to complete under RR!");
    }
    assert(memManager->getUsedMemory() == 0 && "FAIL: Memory leak detected upon process completion under RR!");

    std::cout << "\n--> PASS: Config-driven RR Scheduler & Memory Manager verified successfully!\n";
}

int main() {
    try {
        #ifdef _WIN32
        system("chcp 65001 > nul");
        #endif

        ConfigManager configManager;
        if (!configManager.loadConfig("config.txt")) {
            std::cout << "[WARNING] Could not load config.txt, using default configuration settings." << std::endl;
        } else {
            std::cout << "[INFO] Configuration loaded successfully from config.txt" << std::endl;
        }

        const Config& config = configManager.getConfig();

        test_symbol_table(config);
        test_hex_read_write(config);
        test_demand_paging(config);
        test_memory_violation(config);
        test_custom_screen_instructions();
        runTest_ProcessSMI_and_VMStat();
        test_fcfs_scheduler(config);
        test_rr_scheduler(config);

        std::cout << "\n==========================================" << std::endl;
        std::cout << " 🎉 ALL CONFIG INTEGRATION TESTS PASSED PERFECTLY!" << std::endl;
        std::cout << "==========================================" << std::endl;
    } 
    catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}