#pragma once
#include "CaptureManager.hpp"
#include "VideoProcessor.hpp"
#include "AudioManager.hpp"
#include "AppWindow.hpp"
#include "ConfigManager.hpp"
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
    bool enableAA; // Lanczos4 AA Independiente
    bool enableAI;
    bool enableDenoise; // Filtro de reduccion de ruido IA
    bool showMenu; // Estado UI

    int capWidth = 1920;
    int capHeight = 1080;
    int capFps = 60;
    int srWidth = 3840;
    int srHeight = 2160;
    float denoiseStrength = 0.0f;

    std::vector<DeviceInfo> devices; // Lista de capturadoras

    CaptureManager captureManager;
    VideoProcessor videoProcessor;
    AudioManager audioManager;
    AppWindow appWindow;

    // --- UI Interactions ---
    std::vector<cv::Rect> btnsDevices;
    cv::Rect btnResToggle;
    cv::Rect btnFpsToggle;
    cv::Rect btnDenoiseToggle;
    cv::Rect btnDenoiseStrength;
    cv::Rect btnAAToggle;
    cv::Rect btnAIToggle;
    cv::Rect btnOutToggle;
    cv::Rect btnSave;
    
    bool pendingCaptureRestart;

    void handleInput(bool& captureActive);
    void switchDevice(int newIndex, bool& captureActive);
    void drawMenu(cv::Mat& frame);
    
    static void onMouse(int event, int x, int y, int flags, void* userdata);
    void handleMouse(int event, int x, int y, int flags);
};
