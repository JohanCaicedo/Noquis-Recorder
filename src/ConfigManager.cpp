#include "ConfigManager.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

AppConfig ConfigManager::loadConfig(const std::string& filepath) {
    AppConfig config;
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return config; // Regresa default
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t delimiterPos = line.find('=');
        if (delimiterPos == std::string::npos) continue;

        std::string key = line.substr(0, delimiterPos);
        std::string value = line.substr(delimiterPos + 1);
        
        // Trim
        key.erase(0, key.find_first_not_of(" \t\r\n"));
        key.erase(key.find_last_not_of(" \t\r\n") + 1);
        value.erase(0, value.find_first_not_of(" \t\r\n"));
        value.erase(value.find_last_not_of(" \t\r\n") + 1);

        try {
            if (key == "deviceIndex") config.deviceIndex = std::stoi(value);
            else if (key == "capWidth") config.capWidth = std::stoi(value);
            else if (key == "capHeight") config.capHeight = std::stoi(value);
            else if (key == "capFps") config.capFps = std::stoi(value);
            else if (key == "srWidth") config.srWidth = std::stoi(value);
            else if (key == "srHeight") config.srHeight = std::stoi(value);
            else if (key == "denoiseStrength") config.denoiseStrength = std::stof(value);
            else if (key == "enableDenoise") config.enableDenoise = (value == "1" || value == "true");
            else if (key == "enableAI") config.enableAI = (value == "1" || value == "true");
            else if (key == "enableAA") config.enableAA = (value == "1" || value == "true");
        } catch (...) {
            std::cerr << "[Config] Error parseando valor para la llave: " << key << std::endl;
        }
    }
    return config;
}

void ConfigManager::saveConfig(const std::string& filepath, const AppConfig& config) {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Config] Error abriendo archivo para guardar config." << std::endl;
        return;
    }

    file << "deviceIndex=" << config.deviceIndex << "\n";
    file << "capWidth=" << config.capWidth << "\n";
    file << "capHeight=" << config.capHeight << "\n";
    file << "capFps=" << config.capFps << "\n";
    file << "srWidth=" << config.srWidth << "\n";
    file << "srHeight=" << config.srHeight << "\n";
    file << "denoiseStrength=" << config.denoiseStrength << "\n";
    file << "enableDenoise=" << (config.enableDenoise ? "1" : "0") << "\n";
    file << "enableAI=" << (config.enableAI ? "1" : "0") << "\n";
    file << "enableAA=" << (config.enableAA ? "1" : "0") << "\n";
}
