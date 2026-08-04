#include "CLICONTROL/ScreenSpawnerCommand.h"
#include "memoryControl/MemoryManager.h"
#include "CLICONTROL/InitializeCommand.h"
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
    if (first == std::string::npos) return "";
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

// Check if a number is a power of 2
bool isPowerOfTwo(uint32_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

std::shared_ptr<ICommand> buildInstruction(const std::string& instructionText) {
    std::string instruction = trim(instructionText);
    if (instruction.empty()) return nullptr;

    const std::string upper = toUpperCopy(instruction);

    if (upper.rfind("DECLARE ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, varName, valueToken;
        if (!(iss >> keyword >> varName >> valueToken)) return nullptr;
        try {
            uint16_t value = static_cast<uint16_t>(std::stoul(valueToken));
            return std::make_shared<DeclareCommand>(varName, value);
        } catch (...) { return nullptr; }
    }

    if (upper.rfind("ADD ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, dest, op1, op2;
        if (!(iss >> keyword >> dest >> op1 >> op2)) return nullptr;
        return std::make_shared<AddCommand>(dest, op1, op2);
    }

    if (upper.rfind("SUBTRACT ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, dest, op1, op2;
        if (!(iss >> keyword >> dest >> op1 >> op2)) return nullptr;
        return std::make_shared<SubtractCommand>(dest, op1, op2);
    }

    if (upper.rfind("WRITE ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, addressToken, valueToken;
        if (!(iss >> keyword >> addressToken >> valueToken)) return nullptr;
        return std::make_shared<WriteCommand>(addressToken, valueToken);
    }

    if (upper.rfind("READ ", 0) == 0) {
        std::istringstream iss(instruction);
        std::string keyword, varName, addressToken;
        if (!(iss >> keyword >> varName >> addressToken)) return nullptr;
        return std::make_shared<ReadCommand>(varName, addressToken);
    }

if (upper.rfind("PRINT(", 0) == 0) {
    if (instruction.size() < 7 || instruction.back() != ')') return nullptr;
    std::string inner = instruction.substr(6, instruction.size() - 7);
    std::size_t plusPos = inner.find('+');

    if (plusPos == std::string::npos) {
        std::string trimmedInner = trim(inner);
        
        // If it starts and ends with quotes, it's a string literal e.g. PRINT("Hello")
        if (trimmedInner.size() >= 2 && trimmedInner.front() == '"' && trimmedInner.back() == '"') {
            return std::make_shared<PrintCommand>(stripWrappingQuotes(trimmedInner), "");
        } 
        // Otherwise, it's a variable name e.g. PRINT(varA)
        else {
            return std::make_shared<PrintCommand>("", trimmedInner);
        }
    }

    // Concatenation style e.g. PRINT("Result: " + varA)
    std::string lhs = trim(inner.substr(0, plusPos));
    std::string rhs = trim(inner.substr(plusPos + 1));
    return std::make_shared<PrintCommand>(stripWrappingQuotes(lhs), stripWrappingQuotes(rhs));
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
                if (cmd) commands.push_back(cmd);
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }

    std::string trimmed = trim(current);
    if (!trimmed.empty()) {
        auto cmd = buildInstruction(trimmed);
        if (cmd) commands.push_back(cmd);
    }

    return commands;
}
} // namespace

ScreenSpawnerCommand::ScreenSpawnerCommand() : nextPid(1) {}

bool ScreenSpawnerCommand::execute(const std::string& rawInput, const InitializeCommand& initHandler, MemoryManager& memoryManager, std::mt19937& gen) {
    if (!initHandler.getIsInitialized()) return false;

    std::stringstream ss(rawInput);
    std::string baseCmd, flag, processName;

    ss >> baseCmd >> flag >> processName;

    if (baseCmd != "screen" || processName.empty()) return false;

    const Config& config = initHandler.getConfig();

    // Prevent duplicate process names to avoid backing store file collisions
    {
        std::lock_guard<std::mutex> lock(listMutex);
        for (const auto& proc : activeProcesses) {
            if (proc->getName() == processName) {
                std::cout << "Error: Process with name '" << processName << "' already exists." << std::endl;
                return false;
            }
        }
    }

    std::vector<std::shared_ptr<ICommand>> customCommands;
    uint32_t memorySize = config.minMemPerProc;
    int totalLines = 0;
    bool customInstructionsMode = false;

if (flag == "-c") {
        std::string rest;
        std::getline(ss, rest);
        rest = trim(rest);

        if (rest.empty()) {
            std::cout << "invalid command" << std::endl;
            return false;
        }

        // Peek at the first token to see if an explicit memory size was provided
        std::string firstToken;
        std::stringstream restSS(rest);
        restSS >> firstToken;

        // Check if firstToken consists entirely of digits
        bool isNumeric = !firstToken.empty() && 
                         std::all_of(firstToken.begin(), firstToken.end(), [](unsigned char c) { return std::isdigit(c); });

        if (isNumeric) {
            try {
                memorySize = static_cast<uint32_t>(std::stoul(firstToken));
            } catch (...) {
                std::cout << "invalid command" << std::endl;
                return false;
            }

            // Validate memory bounds and power-of-two
            if (memorySize < config.minMemPerProc || memorySize > config.maxMemPerProc || !isPowerOfTwo(memorySize)) {
                std::cout << "invalid memory allocation" << std::endl;
                return false;
            }

            // Strip the memory size token from the rest of the string
            rest = trim(rest.substr(firstToken.size()));
        } else {
            // No memory size supplied: default to minMemPerProc from config
            memorySize = config.minMemPerProc;
        }

        // Extract the instruction payload wrapped in quotes
        std::string instructionText;
        if (!rest.empty() && rest.front() == '"') {
            rest = rest.substr(1);
            auto quotePos = rest.rfind('"'); // Find the LAST closing quote
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
        std::string memorySizeToken;
        if (!(ss >> memorySizeToken)) {
            std::cout << "invalid memory allocation" << std::endl;
            return false;
        }

        try {
            memorySize = static_cast<uint32_t>(std::stoul(memorySizeToken));
        } catch (...) {
            std::cout << "invalid memory allocation" << std::endl;
            return false;
        }

        // Dynamic validation using config values instead of hardcoded 64 / 65536
        if (memorySize < config.minMemPerProc || memorySize > config.maxMemPerProc || !isPowerOfTwo(memorySize)) {
            std::cout << "invalid memory allocation" << std::endl;
            return false;
        }

        std::uniform_int_distribution<uint32_t> insDist(config.minIns, config.maxIns);
        totalLines = static_cast<int>(insDist(gen));
    } else {
        return false;
    }

    // Instantiate process with config.memPerFrame
    auto newProcess = std::make_shared<Process>(
        nextPid, processName, totalLines, gen, 
        config.varPrint, memorySize, config.memPerFrame, 
        customInstructionsMode ? customCommands : std::vector<std::shared_ptr<ICommand>>{}
    );

    // Verify RAM availability with MemoryManager
    if (!memoryManager.allocateProcessMemory(*newProcess)) {
        std::cout << "Error: Not enough memory available to allocate process " 
                  << processName << " (" << newProcess->getMemorySize() << " bytes required)." << std::endl;
        return false;
    }

    nextPid++;
    {
        std::lock_guard<std::mutex> lock(listMutex);
        activeProcesses.push_back(newProcess);
    }
    return true;
}

const std::vector<std::shared_ptr<Process>>& ScreenSpawnerCommand::getActiveProcesses() const {
    std::lock_guard<std::mutex> lock(listMutex);
    return activeProcesses;
}

const std::vector<std::shared_ptr<Process>>& ScreenSpawnerCommand::getFinishedHistory() const {
    std::lock_guard<std::mutex> lock(listMutex);
    return finishedHistory;
}

void ScreenSpawnerCommand::cleanupFinishedProcesses(MemoryManager& memoryManager) {
    std::lock_guard<std::mutex> lock(listMutex); 
    
    auto it = activeProcesses.begin();
    while (it != activeProcesses.end()) {
        if ((*it)->isFinished()) {
            memoryManager.deallocateProcessMemory(*(*it));
            finishedHistory.push_back(*it);
            it = activeProcesses.erase(it);
        } else {
            ++it;
        }
    }
}