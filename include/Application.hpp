#pragma once
#include "CaptureManager.hpp"
#include "VideoProcessor.hpp"
#include "AudioManager.hpp"
#include "AppWindow.hpp"
#include "ConfigManager.hpp"
#include <vector>
#include <string>

// Win32 Menu IDs
#define IDM_DEVICE_0 1000
#define IDM_DEVICE_MAX 1010

#define IDM_RES_1080 2000
#define IDM_RES_720  2001
#define IDM_FPS_60   2002
#define IDM_FPS_30   2003
#define IDM_MJPG_TOGGLE 2004

#define IDM_DENOISE_TOGGLE 3000
#define IDM_DENOISE_00 3001
#define IDM_DENOISE_05 3002
#define IDM_DENOISE_10 3003

#define IDM_AA_TOGGLE 4000

#define IDM_AI_TOGGLE 5000
#define IDM_TARGET_1080 5001
#define IDM_TARGET_1440 5002
#define IDM_TARGET_2160 5003

#define IDM_SAVE_CONFIG 6000

#define IDM_TAKE_SCREENSHOT 7000

class Application {
public:
    Application(int deviceIndex);
    ~Application();

    // Inicia el bucle principal de la aplicación
    void run();

    // Callback Nativo para Win32
    void handleWin32Command(int menuId);

private:
    int deviceIndex;
    bool isRunning;
    bool enableAA; 
    bool enableAI;
    bool enableDenoise; 
    bool forceMjpg;

    int capWidth = 1920;
    int capHeight = 1080;
    int capFps = 60;
    int srWidth = 3840;
    int srHeight = 2160;
    float denoiseStrength = 0.0f;

    std::vector<DeviceInfo> devices; 

    CaptureManager captureManager;
    VideoProcessor videoProcessor;
    AudioManager audioManager;
    AppWindow appWindow;
    
    bool pendingCaptureRestart;
    bool pendingAIInit;
    bool pendingDenoiseInit;
    bool pendingScreenshot;

    void handleInput(bool& captureActive);
    
    // Win32 Menu Integration
    void setupNativeMenu();
    void updateMenuChecks();
};
