#include "CLICONTROL/ScreenSpawnerCommand.h"
#include "ICommandChildren/DeclareCommand.h"
#include "ICommandChildren/AddCommand.h"
#include "ICommandChildren/SubtractCommand.h"
#include "ICommandChildren/PrintCommand.h"
#include "memoryControl/ReadCommand.h"
#include "memoryControl/WriteCommand.h"
#include <sstream>
#include <random>
#include <algorithm>
#include <mutex>
#include <cctype>
#include <iostream>

namespace {
std::string trim(const std::string& input) {
    size_t first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

std::string toUpperCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return value;
}

std::string stripWrappingQuotes(const std::string& value) {
    std::string copy = trim(value);
    if (copy.size() >= 2 && copy.front() == '"' && copy.back() == '"') {
        return copy.substr(1, copy.size() - 2);
    }
    return copy;
}

std::shared_ptr<ICommand> buildInstruction(const std::string& instructionText) {
    std::string instruction = trim(instructionText);
    if (instruction.empty()) {
        return nullptr;
    }

    const std::string upper = toUpperCopy(instruction);

    if (upper.rfind("DECLARE ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, varName, valueToken;
        if (!(iss >> keyword >> varName >> valueToken)) {
            return nullptr;
        }
        try {
            uint16_t value = static_cast<uint16_t>(std::stoul(valueToken));
            return std::make_shared<DeclareCommand>(varName, value);
        } catch (...) {
            return nullptr;
        }
    }

    if (upper.rfind("ADD ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, dest, op1, op2;
        if (!(iss >> keyword >> dest >> op1 >> op2)) {
            return nullptr;
        }
        return std::make_shared<AddCommand>(dest, op1, op2);
    }

    if (upper.rfind("SUBTRACT ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, dest, op1, op2;
        if (!(iss >> keyword >> dest >> op1 >> op2)) {
            return nullptr;
        }
        return std::make_shared<SubtractCommand>(dest, op1, op2);
    }

    if (upper.rfind("WRITE ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, addressToken, valueToken;
        if (!(iss >> keyword >> addressToken >> valueToken)) {
            return nullptr;
        }
        return std::make_shared<WriteCommand>(addressToken, valueToken);
    }

    if (upper.rfind("READ ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, varName, addressToken;
        if (!(iss >> keyword >> varName >> addressToken)) {
            return nullptr;
        }
        return std::make_shared<ReadCommand>(varName, addressToken);
    }

    if (upper.rfind("PRINT(", 0) == 0) {
        if (instruction.size() < 7 || instruction.back() != ')') {
            return nullptr;
        }

        std::string inner = instruction.substr(6, instruction.size() - 7);
        std::size_t plusPos = inner.find('+');
        if (plusPos == std::string::npos) {
            return std::make_shared<PrintCommand>(stripWrappingQuotes(inner));
        }

        std::string lhs = trim(inner.substr(0, plusPos));
        std::string rhs = trim(inner.substr(plusPos + 1));
        std::string message = stripWrappingQuotes(lhs);
        std::string varName = stripWrappingQuotes(rhs);
        return std::make_shared<PrintCommand>(message, varName);
    }

    return nullptr;
}

std::vector<std::shared_ptr<ICommand>> buildCustomInstructions(const std::string& instructionText) {
    std::vector<std::shared_ptr<ICommand>> commands;
    std::string current;
    bool inQuotes = false;

    for (char ch : instructionText) {
        if (ch == '"') {
            inQuotes = !inQuotes;
            current.push_back(ch);
            continue;
        }

        if (ch == ';' && !inQuotes) {
            std::string trimmed = trim(current);
            if (!trimmed.empty()) {
                auto cmd = buildInstruction(trimmed);
                if (cmd) {
                    commands.push_back(cmd);
                }
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        auto cmd = buildInstruction(trimmed);
        if (cmd) {
            commands.push_back(cmd);
        }
    }

    return commands;
}
} // namespace

ScreenSpawnerCommand::ScreenSpawnerCommand() : nextPid(1) {}

bool ScreenSpawnerCommand::execute(const std::string& rawInput, const InitializeCommand& initHandler, std::mt19937& gen) {
    // Structural guard: System must be initialized to read min/max constraints
    if (!initHandler.getIsInitialized()) {
        return false;
    }

    std::stringstream ss(rawInput);
    std::string baseCmd, flag, processName, memorySizeToken;

    // Extract all 4 tokens universally: "screen" "-s/-c" "<process_name>" "<memory_size>"
    ss >> baseCmd >> flag >> processName >> memorySizeToken;

    if (baseCmd != "screen" || processName.empty() || memorySizeToken.empty()) {
        return false;
    }

    const Config& config = initHandler.getConfig();

    std::vector<std::shared_ptr<ICommand>> customCommands;
    uint32_t memorySize = 4096;
    int totalLines = 0;
    bool customInstructionsMode = false;

    // Safely convert the memory size for both -s and -c flags
    try {
        memorySize = static_cast<uint32_t>(std::stoul(memorySizeToken));
    } catch (...) {
        // main.cpp already printed the error, so we just quietly back out
        return false; 
    }

    if (memorySize < config.minMemPerProc || memorySize > config.maxMemPerProc) {
        std::cout << "Error: Memory size " << memorySize
            << " out of bounds (Min: " << config.minMemPerProc
            << ", Max: " << config.maxMemPerProc << ")." << std::endl;
        return false;
    }

    if (flag == "-c") {
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);

        std::string instructionText;
        if (!rest.empty() && rest[0] == '"') {
            rest = rest.substr(1);
            auto quotePos = rest.find('"');
            if (quotePos == std::string::npos) {
                std::cout << "invalid command" << std::endl;
                return false;
            }
            instructionText = trim(rest.substr(0, quotePos));
        } else {
            instructionText = rest;
        }

        customInstructionsMode = true;
        customCommands = buildCustomInstructions(instructionText);
        if (customCommands.empty() || customCommands.size() > 50) {
            std::cout << "invalid command" << std::endl;
            return false;
        }

        totalLines = static_cast<int>(customCommands.size());
    } else if (flag == "-s") {
        std::uniform_int_distribution<uint32_t> insDist(config.minIns, config.maxIns);
        totalLines = static_cast<int>(insDist(gen));
    } else {
        return false;
    }

    auto newProcess = std::make_shared<Process>(nextPid++, processName, totalLines, gen, config.varPrint, memorySize, 16, customInstructionsMode ? customCommands : std::vector<std::shared_ptr<ICommand>>{});

    {
        std::lock_guard<std::mutex> lock(listMutex);
        activeProcesses.push_back(newProcess);
    }
    return true;
}



const std::vector<std::shared_ptr<Process>>& ScreenSpawnerCommand::getActiveProcesses() const {
    std::lock_guard<std::mutex> lock(listMutex); // Now this will work
    return activeProcesses;
}

const std::vector<std::shared_ptr<Process>>& ScreenSpawnerCommand::getFinishedHistory() const {
    std::lock_guard<std::mutex> lock(listMutex); // Safe access to history too
    return finishedHistory;
}

void ScreenSpawnerCommand::cleanupFinishedProcesses() {
    // Acquire the lock for the entire duration of the cleanup operation
    std::lock_guard<std::mutex> lock(listMutex); 
    
    auto it = activeProcesses.begin();
    while (it != activeProcesses.end()) {
        if ((*it)->isFinished()) {
            finishedHistory.push_back(*it);
            it = activeProcesses.erase(it);
        } else {
            ++it;
        }
    }
}