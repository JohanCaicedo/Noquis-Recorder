#pragma once
#include <string>
#include <map>

struct AppConfig {
    int deviceIndex = -1;
    int capWidth = 1920;
    int capHeight = 1080;
    int capFps = 60;
    int srWidth = 3840;
    int srHeight = 2160;
    float denoiseStrength = 0.0f;
    bool enableDenoise = false;
    bool enableAI = false;
    bool enableAA = false;
    bool forceMjpg = true;
};

class ConfigManager {
public:
    static AppConfig loadConfig(const std::string& filepath);
    static void saveConfig(const std::string& filepath, const AppConfig& config);
};
