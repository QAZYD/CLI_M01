#include "coreDependencies/CommandGenerator.h"

// Concrete command definitions
#include "ICommandChildren/SleepCommand.h" 
#include "ICommandChildren/ForCommand.h"   
#include "ICommandChildren/DeclareCommand.h"
#include "ICommandChildren/AddCommand.h"
#include "ICommandChildren/SubtractCommand.h"
#include "ICommandChildren/PrintCommand.h"

#include <random>
#include <algorithm>

namespace CommandGenerator {

// Updated forward declaration to accept allowVarPrinting configuration
void generateRandomCommandInto(
    std::vector<std::shared_ptr<ICommand>>& program,
    std::mt19937& gen, 
    int currentDepth, 
    int& remainingInstructions, 
    int& generatedInstructions, 
    const std::string& name,
    std::vector<std::string>& declaredVariables,
    int& variableCounter,
    bool allowVarPrinting
);

// Updated signature to take the configuration setting flag
std::vector<std::shared_ptr<ICommand>> generateProgram(int totalLines, std::string name, std::mt19937& gen, bool allowVarPrinting) {
    std::vector<std::shared_ptr<ICommand>> program;
    
    int remainingInstructions = totalLines;
    int generatedInstructions = 0;
    
    std::vector<std::string> declaredVariables;
    int variableCounter = 1;

    while (remainingInstructions > 0 ) {
        generateRandomCommandInto(program, gen, 1, remainingInstructions, generatedInstructions, name, declaredVariables, variableCounter, allowVarPrinting);
    }

    return program;
}

void generateRandomCommandInto(
    std::vector<std::shared_ptr<ICommand>>& program,
    std::mt19937& gen, 
    int currentDepth, 
    int& remainingInstructions, 
    int& generatedInstructions, 
    const std::string& name,
    std::vector<std::string>& declaredVariables,
    int& variableCounter,
    bool allowVarPrinting) 
{
    if (remainingInstructions <= 0) return;

    std::uniform_int_distribution<int> typeDist(0, 5);  
    std::uniform_int_distribution<int> tickDist(1, 10);  
    std::uniform_int_distribution<int> repeatDist(2, 4);
    std::uniform_int_distribution<int> sizeDist(2, 3);   
    std::uniform_int_distribution<int> valDist(1, 50);

    int choice = typeDist(gen);

    if (choice == 0 && currentDepth >= 3) {
        std::uniform_int_distribution<int> nonLoopDist(1, 5);
        choice = nonLoopDist(gen);
    }

    // --- CASE 0: FOR LOOP ---
    if (choice == 0) {
        remainingInstructions--; 
        generatedInstructions++;

        int bodySize = std::min(sizeDist(gen), remainingInstructions);
        std::vector<std::shared_ptr<ICommand>> loopBody;

        for (int i = 0; i < bodySize; ++i) {
            // Pass the allowVarPrinting flag down to nested instructions
            generateRandomCommandInto(loopBody, gen, currentDepth + 1, remainingInstructions, generatedInstructions, name, declaredVariables, variableCounter, allowVarPrinting);
        }

        program.push_back(std::make_shared<ForCommand>(loopBody, repeatDist(gen)));
    }
    // --- CASE 1: SLEEP ---
    else if (choice == 1) {
        remainingInstructions--;
        generatedInstructions++;

        uint8_t randomTicks = static_cast<uint8_t>(tickDist(gen));
        program.push_back(std::make_shared<SleepCommand>(randomTicks));
    } 
    // --- CASE 2-5: OPERATIONAL COMMANDS ---
    else {
        auto ensureVariableCreated = [&](const std::string& varName) {
            if (std::find(declaredVariables.begin(), declaredVariables.end(), varName) == declaredVariables.end()) {
                program.push_back(std::make_shared<DeclareCommand>(varName, 0));
                declaredVariables.push_back(varName);
                remainingInstructions--;
                generatedInstructions++;
            }
        };

        remainingInstructions--;
        generatedInstructions++; 

        switch (choice) {
            case 2: { // DECLARE
                std::string newVar = "v" + std::to_string(variableCounter++);
                declaredVariables.push_back(newVar);
                uint16_t initialVal = static_cast<uint16_t>(valDist(gen));
                program.push_back(std::make_shared<DeclareCommand>(newVar, initialVal));
                break;
            }
            
            case 3: { // ADD
                std::string dest = declaredVariables.empty() ? "v" + std::to_string(variableCounter++) : declaredVariables[std::uniform_int_distribution<int>(0, declaredVariables.size() - 1)(gen)];
                ensureVariableCreated(dest);

                std::string op1 = declaredVariables[std::uniform_int_distribution<int>(0, declaredVariables.size() - 1)(gen)];
                std::string op2 = (typeDist(gen) % 2 == 0) 
                    ? declaredVariables[std::uniform_int_distribution<int>(0, declaredVariables.size() - 1)(gen)] 
                    : std::to_string(valDist(gen));

                program.push_back(std::make_shared<AddCommand>(dest, op1, op2));
                break;
            }
            
            case 4: { // SUBTRACT
                std::string dest = declaredVariables.empty() ? "v" + std::to_string(variableCounter++) : declaredVariables[std::uniform_int_distribution<int>(0, declaredVariables.size() - 1)(gen)];
                ensureVariableCreated(dest);

                std::string op1 = declaredVariables[std::uniform_int_distribution<int>(0, declaredVariables.size() - 1)(gen)];
                std::string op2 = (typeDist(gen) % 2 == 0) 
                    ? declaredVariables[std::uniform_int_distribution<int>(0, declaredVariables.size() - 1)(gen)] 
                    : std::to_string(valDist(gen));

                program.push_back(std::make_shared<SubtractCommand>(dest, op1, op2));
                break;
            }
            
            case 5: 
            default: { // PRINT 
                bool shouldPrintVar = false;

                // Only evaluate variable tracking if configured and variables exist to print
                if (allowVarPrinting && !declaredVariables.empty()) {
                    std::uniform_int_distribution<int> coinFlip(0, 1); // 0 or 1 (50% probability)
                    if (coinFlip(gen) == 0) {
                        shouldPrintVar = true;
                    }
                }

                if (shouldPrintVar) {
                    // Pick a random already-declared variable from the tracking array
                    std::uniform_int_distribution<int> varIdxDist(0, declaredVariables.size() - 1);
                    std::string chosenVar = declaredVariables[varIdxDist(gen)];
                    
                    program.push_back(std::make_shared<PrintCommand>("Value from " + name + ": ", chosenVar));
                } else {
                    // Fall back to standard literal print statement
                    program.push_back(std::make_shared<PrintCommand>("Hello world from " + name + "!"));
                }
                break;
            }
        }
    }
}

} // namespace CommandGenerator