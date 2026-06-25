#ifndef COMMAND_GENERATOR_H
#define COMMAND_GENERATOR_H

#include <vector>
#include <memory>
#include <random>
#include <string>

// Forward declaration of ICommand to avoid deep include chaining in headers
class ICommand;

namespace CommandGenerator {

    // Public API: Generates a complete vector of randomized commands up to totalLines
    std::vector<std::shared_ptr<ICommand>> generateProgram(int totalLines);

    // Internal helper: Recursively generates individual commands or nested loops
    std::shared_ptr<ICommand> generateRandomCommand(
        std::mt19937& gen, 
        int currentDepth, 
        int& remainingInstructions, 
        int& generatedInstructions
    );

} // namespace CommandGenerator

#endif // COMMAND_GENERATOR_H