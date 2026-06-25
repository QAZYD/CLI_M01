#include "coreDependencies/CommandGenerator.h"

// Concrete command definitions
#include "ICommandChildren/SleepCommand.h" 
#include "ICommandChildren/ForCommand.h"   
#include "ICommandChildren/DeclareCommand.h"
#include "ICommandChildren/AddCommand.h"
#include "ICommandChildren/SubtractCommand.h"
#include "ICommandChildren/PrintCommand.h"

namespace CommandGenerator {

std::vector<std::shared_ptr<ICommand>> generateProgram(int totalLines) {
    std::vector<std::shared_ptr<ICommand>> program;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int remainingInstructions = totalLines;
    int generatedInstructions = 0;

    while (generatedInstructions < remainingInstructions) {
        program.push_back(
            generateRandomCommand(gen, 1, remainingInstructions, generatedInstructions)
        );
    }

    return program;
}

std::shared_ptr<ICommand> generateRandomCommand(
    std::mt19937& gen, 
    int currentDepth, 
    int& remainingInstructions, 
    int& generatedInstructions) 
{
    std::uniform_int_distribution<int> typeDist(0, 5);  
    std::uniform_int_distribution<int> tickDist(1, 10);  
    std::uniform_int_distribution<int> repeatDist(2, 4);
    std::uniform_int_distribution<int> sizeDist(2, 3);   

    int choice = typeDist(gen);

    // Enforce the max loop nesting rule (up to 3 times)
    if (choice == 0 && currentDepth >= 3) {
        std::uniform_int_distribution<int> nonLoopDist(1, 5);
        choice = nonLoopDist(gen);
    }

    if (choice == 1) {
        remainingInstructions--;
        generatedInstructions++;

        uint8_t randomTicks = static_cast<uint8_t>(tickDist(gen));
        return std::make_shared<SleepCommand>(randomTicks);
    } 
    else if (choice == 0) {
        remainingInstructions--;  // FOR counts as one
        generatedInstructions++;

        int bodySize = std::min(sizeDist(gen), remainingInstructions);
        std::vector<std::shared_ptr<ICommand>> loopBody;

        for (int i = 0; i < bodySize; ++i) {
            loopBody.push_back(
                generateRandomCommand(gen, currentDepth + 1, remainingInstructions, generatedInstructions)
            );
        }

        return std::make_shared<ForCommand>(loopBody, repeatDist(gen));
    }
    else {
        switch (choice) {
            case 2: 
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<DeclareCommand>("mockVar", 0); 
            
            case 3: 
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<AddCommand>("mockVar", "mockVar", "1");
            
            case 4: 
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<SubtractCommand>("mockVar", "mockVar", "1");
            
            case 5: 
            default:
                remainingInstructions--;
                generatedInstructions++; 
                return std::make_shared<PrintCommand>("mockVar");
        }
    }
}

} // namespace CommandGenerator