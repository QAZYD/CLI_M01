/* #include <iostream>
#include <string>
#include <conio.h>
#include <windows.h>
#include "banner.h"
#include "CommandProcessor.h" // Include your new OOP file

int main() {
    system("cls");
    showBanner();

    CommandProcessor processor; // Instantiate the object
    std::string inputBuffer;
    bool running = true;

    std::cout << "root:\\> ";

    while (running) {
        if (_kbhit()) {
            int ch = _getch();

            if (ch == '\r' || ch == '\n') {
                std::cout << std::endl;

                // OOP handling takes care of trimming and command execution internally
                running = processor.execute(inputBuffer);
                inputBuffer.clear();

                if (running) {
                    std::cout << "root:\\> ";
                }

            } else if (ch == '\b') {
                if (!inputBuffer.empty()) {
                    inputBuffer.pop_back();
                    std::cout << "\b \b";
                }

            } else if (ch == 0 || ch == 224) {
                _getch(); // Skip extended key codes

            } else if (ch == 3) { // Ctrl+C handling
                running = false;

            } else if (ch >= 32 && ch <= 126) {
                inputBuffer += static_cast<char>(ch);
                std::cout << static_cast<char>(ch);
            }
        }

        Sleep(10);
    }

    return 0;
} */
#include <iostream>
#include <memory>
#include <vector>

// Include the foundational layers
#include "coreDependencies/SymbolTable.h"
#include "coreDependencies/ICommand.h"
#include "ProcessControl.h"

// Include the command workers
#include "ICommandChildren/DeclareCommand.h"
#include "ICommandChildren/PrintCommand.h"
#include "ICommandChildren/AddCommand.h"
#include "ICommandChildren/SubtractCommand.h"

int main() {
    std::cout << "=== OS Process Control Simulation ===" << std::endl;

    // 1. Instantiate a new isolated process (PID: 101, Name: "MathKernel")
    Process myProcess(101, "MathKernel");
    
    std::cout << "Created Process: " << myProcess.getName() 
              << " [PID: " << myProcess.getPID() << "]" << std::endl;
    std::cout << "Initial State: " << myProcess.getState() << " (0 = READY)\n" << std::endl;

    // 2. Queue up a sequence of commands into the process's private memory
    // Simulated Script:
    //   DECLARE x = 50
    //   DECLARE y = 20
    //   ADD x = x + y  (or whatever your Add implementation stores)
    //   PRINT x
    myProcess.addCommand(std::make_shared<DeclareCommand>("x", 50));
    myProcess.addCommand(std::make_shared<DeclareCommand>("y", 20));
    
    // Adjust these strings to match how your Add/Subtract/Print constructors are written:
    myProcess.addCommand(std::make_shared<AddCommand>("x", "x", "y")); 
    myProcess.addCommand(std::make_shared<PrintCommand>("x"));

    std::cout << "Instructions successfully loaded into program memory." << std::endl;
    std::cout << "Starting CPU Execution Loop...\n" << std::endl;
    std::cout << "---------------------------------------" << std::endl;

    // 3. The CPU/Scheduler Simulation Loop
    // This loop ticks forward step-by-step until the process reports it is FINISHED
    int tick = 1;
    while (!myProcess.isFinished()) {
        std::cout << "[Tick " << tick << "] State: " << myProcess.getState() << " | Executing... " << std::endl;
        
        // Execute the exact line the process control block is pointing to
        myProcess.executeCurrentCommand();
        
        // Advance the program counter forward to the next line
        myProcess.moveToNextLine();
        
        tick++;
    }

    std::cout << "---------------------------------------" << std::endl;
    std::cout << "\nExecution complete!" << std::endl;
    std::cout << "Final Process State: " << myProcess.getState() << " (3 = FINISHED)" << std::endl;

    return 0;
}