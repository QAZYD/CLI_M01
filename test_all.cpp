#include <iostream>
#include <cassert>
#include <memory>
#include <random>
#include <fstream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector>

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

// Helper function to map Process state enum to readable text
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

// ---------------------------------------------------------
// TEST 1: SYMBOL TABLE CONSTRAINTS (CAPACITY & CLAMPING)
// ---------------------------------------------------------
void test_symbol_table(const Config& config) {
    printHeader("TEST 1: SYMBOL TABLE (32-VAR CAP & CLAMPING)");
    SymbolTable st;

    // 1. Test Clamping above 65535
    st.set("bigVar", 70000);
    std::cout << "[1.1] Clamping Check: Set 'bigVar' to 70000 -> Stored: " << st.get("bigVar") << std::endl;
    assert(st.get("bigVar") == 65535 && "FAIL: Value was not clamped to 65535!");

    // 2. Fill Symbol Table to 32 variables
    std::cout << "[1.2] Filling Symbol Table up to 32 variables..." << std::endl;
    for (int i = 0; i < 31; ++i) { // bigVar is #1, add 31 more -> total 32
        st.declare("var_" + std::to_string(i), i * 10);
    }
    assert(st.isFull() && "FAIL: Symbol Table should be full at 32 variables!");

    // 3. Attempt to add 33rd variable (Should be ignored)
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
    memManager.initialize(128, 16); // 128 bytes total RAM, 16-byte frames

    std::mt19937 gen(42);
    Process p1(1, "Proc_A", 10, gen, false); // Memory size defaults to 4096 bytes

    // 1. Write value 42 to hex address 0x0004
    WriteCommand writeCmd1("0x0004", "42");
    writeCmd1.execute(p1, memManager);
    std::cout << "[2.1] Executed: " << writeCmd1.toString() << std::endl;

    // 2. Read back from address 0x0004 into variable 'resultVar'
    ReadCommand readCmd1("resultVar", "0x0004");
    readCmd1.execute(p1, memManager);
    std::cout << "[2.2] Executed: " << readCmd1.toString() << std::endl;

    uint16_t valRead = p1.getSymbolTable().get("resultVar");
    std::cout << "      -> Value in SymbolTable ('resultVar'): " << valRead << std::endl;
    assert(valRead == 42 && "FAIL: Read value does not match written value!");

    // 3. Write value from variable to another memory address (0x000E)
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

    // Micro-RAM: 32 Bytes Total, 16 Byte Frames = EXACTLY 2 Physical Frames!
    MemoryManager memManager;
    memManager.initialize(32, 16);

    std::mt19937 gen(42);
    Process p1(1, "Proc_Paging", 10, gen, false);

    std::cout << "[3.1] Initial RAM Status -> Free Memory: " << memManager.getFreeMemory() << " bytes\n";

    // Fill Frame 0 (Virtual Page 0 -> addr 0x0000)
    WriteCommand("0x0000", "1111").execute(p1, memManager);
    
    // Fill Frame 1 (Virtual Page 1 -> addr 0x0010)
    WriteCommand("0x0010", "2222").execute(p1, memManager);

    std::cout << "[3.2] RAM full (2/2 frames used). Free Memory: " << memManager.getFreeMemory() << " bytes\n";

    // Access Page 2 (addr 0x0020) -> Forces Page Fault & LRU Eviction of Page 0!
    std::cout << "[3.3] Triggering Page Fault on Virtual Page 2 (addr 0x0020)...\n";
    WriteCommand("0x0020", "3333").execute(p1, memManager);

    std::cout << "      -> Total Pages Paged In:  " << memManager.getPagesPagedIn() << std::endl;
    std::cout << "      -> Total Pages Paged Out: " << memManager.getPagesPagedOut() << std::endl;

    assert(memManager.getPagesPagedOut() > 0 && "FAIL: Page Eviction was not triggered!");

    // Verify backing store file existence
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
    memManager.initialize(64, 16);

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
    std::string command = "screen -c custom_proc 64 \"DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT(Result + varC)\"";

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

// ---------------------------------------------------------
// TEST 6: HEAVY FCFS SCHEDULER & MEMORY STRESS INTEGRATION
// ---------------------------------------------------------
void test_fcfs_scheduler(const Config& config) {
    printHeader("HEAVY TEST 6: FCFS SCHEDULER & MEMORY STRESS INTEGRATION");

    // RAM: 64 bytes total, 16-byte frames = 4 Physical RAM Frames
    auto memManager = std::make_shared<MemoryManager>();
    memManager->initialize(64, 16);

    // 2 CPU Cores, 1 delay cycle per instruction
    int totalCores = 2;
    int execDelay = 1;
    FCFSScheduler scheduler(totalCores, execDelay, memManager);

    std::mt19937 gen(1337);
    InitializeCommand initHandler("config.txt");
    initHandler.execute();

    ScreenSpawnerCommand spawner;

    // Spawn 6 Heavy Processes competing for 4 RAM frames across 2 CPU Cores
    std::vector<std::string> processScripts = {
        "screen -c Proc_FCFS_1 128 \"WRITE 0x0000 10; WRITE 0x0010 20; WRITE 0x0020 30; SLEEP 1; WRITE 0x0030 40; WRITE 0x0040 50; WRITE 0x0050 60\"",
        "screen -c Proc_FCFS_2 128 \"DECLARE vA 100; WRITE 0x0010 vA; READ vB 0x0010; SLEEP 1; WRITE 0x0050 vB; WRITE 0x0060 999\"",
        "screen -c Proc_FCFS_3 128 \"WRITE 0x0000 300; WRITE 0x0020 302; SLEEP 1; WRITE 0x0040 304; WRITE 0x0060 306\"",
        "screen -c Proc_FCFS_4 128 \"WRITE 0x0010 401; WRITE 0x0030 403; SLEEP 2; WRITE 0x0050 405; WRITE 0x0070 407\"",
        "screen -c Proc_FCFS_5 128 \"DECLARE x 5; DECLARE y 10; ADD x x y; WRITE 0x0000 x; WRITE 0x0070 x\"",
        "screen -c Proc_FCFS_6 128 \"WRITE 0x0020 602; WRITE 0x0030 603; WRITE 0x0040 604; SLEEP 1; WRITE 0x0050 605\""
    };

    std::vector<std::shared_ptr<Process>> activeProcesses;
    for (const auto& script : processScripts) {
        spawner.execute(script, initHandler, gen);
        activeProcesses.push_back(spawner.getActiveProcesses().back());
    }

    std::cout << "[6.1] System Config (Heavy Load):\n";
    std::cout << "      • Cores: " << totalCores << " | Execution Delay: " << execDelay << " cycle(s)\n";
    std::cout << "      • Active Processes: " << activeProcesses.size() << " processes\n";
    std::cout << "      • RAM Capacity: " << memManager->getTotalMemory() << " bytes (" 
              << (memManager->getTotalMemory() / 16) << " frames @ 16B each)\n\n";

    std::cout << "[6.2] Enqueuing 6 Heavy Processes into FCFS Ready Queue...\n";
    for (const auto& proc : activeProcesses) {
        scheduler.pushProcess(proc);
    }

    std::cout << "[6.3] Starting FCFS Scheduler Threads...\n";
    scheduler.start();

    // Monitoring Loop with 10s Timeout for Heavy Load
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
                      << "RAM Used: " << std::setw(2) << memManager->getUsedMemory() << "/" << memManager->getTotalMemory() << "B | "
                      << "Free: " << std::setw(2) << memManager->getFreeMemory() << "B | "
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

    // Diagnostic Summary & Assertions
    std::cout << "[6.4] Final Heavy Workload Diagnostic Summary:\n";
    std::cout << "      • Total CPU Cycles Elapsed : " << scheduler.getCPUCycles() << "\n";
    std::cout << "      • Total Pages Paged In     : " << memManager->getPagesPagedIn() << "\n";
    std::cout << "      • Total Pages Paged Out    : " << memManager->getPagesPagedOut() << "\n";
    std::cout << "      • Remaining RAM In-Use     : " << memManager->getUsedMemory() << " bytes (Free: " 
              << memManager->getFreeMemory() << " bytes)\n";

    for (size_t i = 0; i < activeProcesses.size(); ++i) {
        assert(activeProcesses[i]->isFinished() && "FAIL: A process failed to complete under FCFS heavy load!");
    }
    assert(memManager->getPagesPagedOut() > 0 && "FAIL: Demand paging/eviction was not triggered under heavy memory pressure!");
    assert(memManager->getUsedMemory() == 0 && "FAIL: Memory leak! RAM was not fully reclaimed upon process completion!");

    std::cout << "\n--> PASS: Heavy FCFS Scheduler & Memory Manager integration verified successfully!\n";
}

// ---------------------------------------------------------
// TEST 7: HEAVY RR SCHEDULER & CONTEXT SWITCHING STRESS
// ---------------------------------------------------------
void test_rr_scheduler(const Config& config) {
    printHeader("HEAVY TEST 7: RR SCHEDULER & CONTEXT SWITCHING STRESS");

    // Extreme Memory Pressure: 32 bytes RAM (ONLY 2 Physical Frames)
    auto memManager = std::make_shared<MemoryManager>();
    memManager->initialize(32, 16);

    // 2 CPU Cores, 0 delay, Quantum = 2 cycles (Forced frequent context switches)
    int totalCores = 2;
    int execDelay = 0;
    int quantum = 2;
    RRScheduler scheduler(totalCores, execDelay, quantum, memManager);

    std::mt19937 gen(1337);
    InitializeCommand initHandler("config.txt");
    initHandler.execute();

    ScreenSpawnerCommand spawner;

    // Spawn 6 Heavy Processes forcing frequent quantum preemption + severe page thrashing
    std::vector<std::string> processScripts = {
        "screen -c Proc_RR_1 128 \"WRITE 0x0000 11; WRITE 0x0010 12; WRITE 0x0020 13; SLEEP 1; WRITE 0x0030 14; WRITE 0x0000 15; PRINT(P1_Done)\"",
        "screen -c Proc_RR_2 128 \"WRITE 0x0020 21; WRITE 0x0030 22; SLEEP 1; WRITE 0x0040 23; WRITE 0x0010 24; PRINT(P2_Done)\"",
        "screen -c Proc_RR_3 128 \"DECLARE a 5; DECLARE b 15; ADD a a b; WRITE 0x0000 a; WRITE 0x0050 a; SLEEP 1; WRITE 0x0020 a; PRINT(P3_Done)\"",
        "screen -c Proc_RR_4 128 \"WRITE 0x0040 41; WRITE 0x0050 42; WRITE 0x0010 43; SLEEP 1; WRITE 0x0030 44; PRINT(P4_Done)\"",
        "screen -c Proc_RR_5 128 \"WRITE 0x0000 51; WRITE 0x0020 52; WRITE 0x0040 53; WRITE 0x0010 54; PRINT(P5_Done)\"",
        "screen -c Proc_RR_6 128 \"WRITE 0x0030 61; WRITE 0x0050 62; SLEEP 1; WRITE 0x0000 63; PRINT(P6_Done)\""
    };

    std::vector<std::shared_ptr<Process>> activeProcesses;
    for (const auto& script : processScripts) {
        spawner.execute(script, initHandler, gen);
        activeProcesses.push_back(spawner.getActiveProcesses().back());
    }

    std::cout << "[7.1] System Config (Heavy Round-Robin Load):\n";
    std::cout << "      • Cores: " << totalCores << " | Delay: " << execDelay << " cycle(s) | Quantum: " << quantum << " cycle(s)\n";
    std::cout << "      • Active Processes: " << activeProcesses.size() << " processes\n";
    std::cout << "      • RAM Capacity: " << memManager->getTotalMemory() << " bytes (" 
              << (memManager->getTotalMemory() / 16) << " frames @ 16B each)\n\n";

    std::cout << "[7.2] Enqueuing 6 Heavy Processes into Round-Robin Ready Queue...\n";
    for (const auto& proc : activeProcesses) {
        scheduler.pushProcess(proc);
    }

    std::cout << "[7.3] Starting Round-Robin Scheduler Threads...\n";
    scheduler.start();

    // Monitoring Loop with 10s Timeout
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
                      << "RAM Used: " << std::setw(2) << memManager->getUsedMemory() << "/" << memManager->getTotalMemory() << "B | "
                      << "Free: " << std::setw(2) << memManager->getFreeMemory() << "B | "
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

    // Diagnostic Summary & Assertions
    std::cout << "[7.4] Final Heavy Workload Diagnostic Summary:\n";
    std::cout << "      • Total CPU Cycles Elapsed : " << scheduler.getCPUCycles() << "\n";
    std::cout << "      • Total Pages Paged In     : " << memManager->getPagesPagedIn() << "\n";
    std::cout << "      • Total Pages Paged Out    : " << memManager->getPagesPagedOut() << "\n";
    std::cout << "      • Remaining RAM In-Use     : " << memManager->getUsedMemory() << " bytes (Free: " 
              << memManager->getFreeMemory() << " bytes)\n";

    for (size_t i = 0; i < activeProcesses.size(); ++i) {
        assert(activeProcesses[i]->isFinished() && "FAIL: A process failed to complete under RR heavy load!");
    }
    assert(memManager->getPagesPagedOut() > 0 && "FAIL: Demand paging/eviction was not triggered under heavy RR load!");
    assert(memManager->getUsedMemory() == 0 && "FAIL: Memory leak detected upon process completion under RR!");

    std::cout << "\n--> PASS: Heavy RR Scheduler & Context Switching stress verified successfully!\n";
}

int main() {
    try {
        // Fix Windows Console UTF-8 Symbol Display (e.g. arrows, bullets)
        #ifdef _WIN32
        system("chcp 65001 > nul");
        #endif

        // Initialize ConfigManager and load configuration
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
        test_fcfs_scheduler(config);
        test_rr_scheduler(config);

        std::cout << "\n==========================================" << std::endl;
        std::cout << " 🎉 ALL INTEGRATION TESTS PASSED PERFECTLY!" << std::endl;
        std::cout << "==========================================" << std::endl;
    } 
    catch (const std::exception& e) {
        std::cerr << "\n❌ TEST FAILED WITH EXCEPTION: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}