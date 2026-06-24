#pragma once

#include "Config.h"
#include <string>

class ConfigManager {
public:
    bool loadConfig(const std::string& filename);

    const Config& getConfig() const;

private:
    Config config;
};