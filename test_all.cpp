#include <iostream>
#include <cassert>
#include <memory>
#include <random>
#include <fstream>

#include "memoryControl/MemoryManager.h"
#include "coreDependencies/ProcessControl.h"
#include "coreDependencies/SymbolTable.h"
#include "coreDependencies/configManager.h"
#include "memoryControl/ReadCommand.h"
#include "memoryControl/WriteCommand.h"
#include "CLICONTROL/ScreenSpawnerCommand.h"
#include "CLICONTROL/InitializeCommand.h"

void printHeader(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
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
    // Note: p1 memory size in constructor default is 4096 bytes (valid address range: 0x0000 - 0x0FFF)

    // Attempting to write to address 0x5000 (Out of bounds for 4096-byte process!)
    std::cout << "[4.1] Executing out-of-bounds WRITE to address 0x5000...\n";
    WriteCommand badCmd("0x5000", "9999");
    badCmd.execute(p1, memManager);

    std::cout << "      -> Process State: " 
              << (p1.getState() == Process::ProcessState::MEMORY_VIOLATION ? "MEMORY_VIOLATION" : "OTHER") 
              << std::endl;

    assert(p1.getState() == Process::ProcessState::MEMORY_VIOLATION && "FAIL: Process did not enter MEMORY_VIOLATION state!");
    std::cout << "      -> Recorded Fault Address: 0x" << std::hex << p1.getInvalidAddress() << std::dec << std::endl;
    std::cout << "      -> Recorded Fault Timestamp: " << p1.getErrorTimestamp() << std::endl;

    // Test that process refuses further command execution after memory violation
    std::cout << "[4.2] Verifying process halts further execution...\n";
    int linesBefore = p1.getLinesExecuted();
    p1.executeCurrentCommand(memManager);
    assert(p1.getLinesExecuted() == linesBefore && "FAIL: Process executed instructions after memory violation!");

    std::cout << "--> PASS: Out-of-bounds memory violation handled successfully!\n";
}

void test_custom_screen_instructions() {
    printHeader("TEST 5: USER-DEFINED SCREEN INSTRUCTIONS");

    InitializeCommand initHandler("config.txt");
    assert(initHandler.execute() && "FAIL: Could not initialize config for custom instruction test!");

    std::mt19937 gen(42);
    ScreenSpawnerCommand spawner;
    std::string command = "screen -c custom_proc 64 \"DECLARE varA 10; DECLARE varB 5; ADD varA varA varB; WRITE 0x500 varA; READ varC 0x500; PRINT(\"Result: \" + varC)\"";

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

int main() {
    try {
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