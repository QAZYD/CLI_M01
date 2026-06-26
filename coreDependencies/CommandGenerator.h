#ifndef COMMAND_GENERATOR_H
#define COMMAND_GENERATOR_H

#include <vector>
#include <memory>
#include <random>
#include <string>

// Forward declaration of ICommand to avoid deep include chaining in headers
class ICommand;

namespace CommandGenerator {

    // Public API: Now accepts the shared random engine by reference
    std::vector<std::shared_ptr<ICommand>> generateProgram(
        int totalLines, 
        std::string name, 
        std::mt19937& gen,
        bool allowVarPrinting
    );

    // Internal helper: Mutates the program vector directly to handle safe implicit auto-0 declarations
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

} // namespace CommandGenerator

#endif // COMMAND_GENERATOR_H