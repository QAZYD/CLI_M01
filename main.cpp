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
#include "SymbolTable.h"
#include "ICommand.h"
// Include your command implementations here
// (If you placed them in separate files, include them all)
#include "DeclareCommand.h"
#include "PrintCommand.h"
#include "AddCommand.h"
#include "SubtractCommand.h"

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "        RUNNING COMPONENT TESTER        " << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. Create the Symbol Table instance
    SymbolTable globalTable;

    // 2. Create a list of instructions using smart pointers
    std::vector<std::shared_ptr<ICommand>> script;

    // Test Case A: Standard Declaration and Printing
    std::cout << "\n[Loading Instructions...]" << std::endl;
    script.push_back(std::make_shared<DeclareCommand>("x", 10));
    script.push_back(std::make_shared<DeclareCommand>("y", 5));
    
    // Test Case B: Mathematical operations (Variables only)
    script.push_back(std::make_shared<AddCommand>("sum", "x", "y")); // sum = 10 + 5
    
    // Test Case C: Mathematical operations (Mixing variables and raw values)
    script.push_back(std::make_shared<AddCommand>("total", "sum", "20")); // total = 15 + 20
    
    // Test Case D: Subtraction and Underflow prevention
    script.push_back(std::make_shared<SubtractCommand>("diff", "y", "x")); // diff = 5 - 10 (Should result in 0, not negative/underflow)

    // Test Case E: Testing the "Auto-Declare to 0" rule
    // "ghostVar" has never been declared, so printing it should auto-create it as 0
    script.push_back(std::make_shared<PrintCommand>("Testing auto-declare (ghostVar): ", "ghostVar"));

    // Adding prints to verify all calculations
    script.push_back(std::make_shared<PrintCommand>("Value of x: ", "x"));
    script.push_back(std::make_shared<PrintCommand>("Value of y: ", "y"));
    script.push_back(std::make_shared<PrintCommand>("Addition (x + y): ", "sum"));
    script.push_back(std::make_shared<PrintCommand>("Addition with raw value (sum + 20): ", "total"));
    script.push_back(std::make_shared<PrintCommand>("Subtraction underflow check (y - x): ", "diff"));

    // 3. Simulate execution loop (This is exactly what process.h will do!)
    std::cout << "\n[Executing Script Context...]" << std::endl;
    for (const auto& command : script) {
        command->execute(globalTable);
    }

    std::cout << "\n========================================" << std::endl;
    std::cout << "          TEST COMPLETION CHECK         " << std::endl;
    std::cout << "========================================" << std::endl;
    
    // Double-check the SymbolTable backend directly via code
    if (globalTable.get("sum") == 15 && globalTable.get("ghostVar") == 0 && globalTable.get("diff") == 0) {
        std::cout << ">> SUCCESS: SymbolTable and ICommands are working perfectly!" << std::endl;
        std::cout << ">> You are officially ready to build process.cpp." << std::endl;
    } else {
        std::cout << ">> FAILURE: One of the output states did not match expected values." << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return 0;
}