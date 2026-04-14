#pragma once
#include "CaptureManager.hpp"
#include "VideoProcessor.hpp"
#include "AudioManager.hpp"
#include "AppWindow.hpp"
#include <vector>
#include <string>

class Application {
public:
    Application(int deviceIndex);
    ~Application();

    // Inicia el bucle principal de la aplicación
    void run();

private:
    int deviceIndex;
    bool isRunning;
    bool enableAI;
    bool enableDenoise; // Filtro de reduccion de ruido IA
    bool showMenu; // Estado UI

    int capWidth = 1920;
    int capHeight = 1080;
    int srWidth = 2560;
    int srHeight = 1440;

    std::vector<DeviceInfo> devices; // Lista de capturadoras

    CaptureManager captureManager;
    VideoProcessor videoProcessor;
    AudioManager audioManager;
    AppWindow appWindow;

    void handleInput(bool& captureActive);
    void switchDevice(int newIndex, bool& captureActive);
    void drawMenu(cv::Mat& frame);
};
