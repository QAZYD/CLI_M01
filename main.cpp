#include "coreDependencies/CommandProcessor.h"
#include "coreDependencies/banner.h"
#include <iostream>
#include <string>

int main() {
    CommandProcessor processor;
    std::string userInput;
    bool isRunning = true;

    // 2. Call the modular banner function
    showBanner();

    // Continuous execution shell loop
    while (isRunning) {
        std::cout << "root:\\ ";
        
        if (!std::getline(std::cin, userInput)) {
            break; 
        }

        isRunning = processor.execute(userInput);
    }

    std::cout << "Shell session terminated cleanly. Goodbye!\n";
    return 0;
}