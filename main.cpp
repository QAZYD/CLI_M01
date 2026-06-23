#include <iostream>
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
                    std::cout << "opesy terminal:\\> ";
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
}