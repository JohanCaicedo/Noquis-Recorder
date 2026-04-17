#include "Application.hpp"
#include <iostream>
#include <windows.h>
#include <chrono>
#include <fstream>

// Global variables for Win32 hook
Application* g_appInstance = nullptr;
WNDPROC g_oldWndProc = nullptr;
HWND g_hwnd = nullptr;
HMENU g_hMenu = nullptr;

LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_COMMAND) {
        int menuId = LOWORD(wParam);
        if (g_appInstance) {
            g_appInstance->handleWin32Command(menuId);
        }
    }
    // Call the original OpenCV window procedure
    return CallWindowProc(g_oldWndProc, hwnd, uMsg, wParam, lParam);
}

Application::Application(int deviceIndex) 
    : isRunning(false), enableAA(false), enableAI(false), enableDenoise(false), 
      appWindow("Visor"), pendingCaptureRestart(false), pendingAIInit(false), pendingDenoiseInit(false), pendingScreenshot(false) {
    
    devices = CaptureManager::getAvailableDevices();

    AppConfig cfg = ConfigManager::loadConfig("config.ini");
    if (cfg.deviceIndex != -1) {
        this->deviceIndex = cfg.deviceIndex;
        this->capWidth = cfg.capWidth;
        this->capHeight = cfg.capHeight;
        this->capFps = cfg.capFps;
        this->srWidth = cfg.srWidth;
        this->srHeight = cfg.srHeight;
        this->denoiseStrength = cfg.denoiseStrength;
        this->enableDenoise = cfg.enableDenoise;
        this->enableAI = cfg.enableAI;
        this->enableAA = cfg.enableAA;
        this->forceMjpg = cfg.forceMjpg;
    } else {
        this->deviceIndex = deviceIndex;
        this->forceMjpg = true;
    }

    g_appInstance = this;
}

Application::~Application() {
    captureManager.release();
    g_appInstance = nullptr;
}

void Application::setupNativeMenu() {
    // Find the native OpenCV window by its Title created in AppWindow
    g_hwnd = FindWindowA(NULL, "Visor");
    if (!g_hwnd) {
        std::cerr << "[Win32] No se encontro la ventana para inyectar el menu nativo." << std::endl;
        return;
    }

    g_hMenu = CreateMenu();

    // 1. Menu Dispositivos
    HMENU hMenuDevice = CreatePopupMenu();
    for (size_t i = 0; i < devices.size(); i++) {
        AppendMenuA(hMenuDevice, MF_STRING, IDM_DEVICE_0 + i, devices[i].name.c_str());
    }
    if (devices.empty()) AppendMenuA(hMenuDevice, MF_STRING | MF_GRAYED, 0, "Sin dispositivos");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuDevice, "Dispositivo");

    // 2. Menu Entrada
    HMENU hMenuInput = CreatePopupMenu();
    AppendMenuA(hMenuInput, MF_STRING, IDM_RES_1080, "Resolucion 1080p");
    AppendMenuA(hMenuInput, MF_STRING, IDM_RES_720, "Resolucion 720p");
    AppendMenuA(hMenuInput, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenuInput, MF_STRING, IDM_FPS_60, "Framerates 60 FPS");
    AppendMenuA(hMenuInput, MF_STRING, IDM_FPS_30, "Framerates 30 FPS");
    AppendMenuA(hMenuInput, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenuInput, MF_STRING, IDM_MJPG_TOGGLE, "Forzar Ingesta MJPEG (Recomendado)");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuInput, "Ingesta (USB)");

    // 3. Menu IA Procesamiento
    HMENU hMenuAI = CreatePopupMenu();
    
    // 3.1 Denoise Submenu
    HMENU hMenuDenoise = CreatePopupMenu();
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_TOGGLE, "Activar Denoise");
    AppendMenuA(hMenuDenoise, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_00, "Denoise: Apagado");
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_05, "Denoise: Bajo");
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_10, "Denoise: Alto");
    AppendMenuA(hMenuAI, MF_POPUP, (UINT_PTR)hMenuDenoise, "NVIDIA Denoise");

    // 3.2 AntiAliasing Tradicional
    AppendMenuA(hMenuAI, MF_STRING, IDM_AA_TOGGLE, "AA Lanczos 4");

    // 3.3 Super Resolución
    HMENU hMenuSR = CreatePopupMenu();
    AppendMenuA(hMenuSR, MF_STRING, IDM_AI_TOGGLE, "Activar RTX_VSR");
    AppendMenuA(hMenuSR, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenuSR, MF_STRING, IDM_TARGET_1080, "Objetivo 1080p");
    AppendMenuA(hMenuSR, MF_STRING, IDM_TARGET_1440, "Objetivo 1440p");
    AppendMenuA(hMenuSR, MF_STRING, IDM_TARGET_2160, "Objetivo 4K");
    AppendMenuA(hMenuAI, MF_POPUP, (UINT_PTR)hMenuSR, "Super Res (NVIDIA)");

    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuAI, "Procesamiento IA");

    // 4. Menu Settings
    HMENU hMenuSettings = CreatePopupMenu();
    AppendMenuA(hMenuSettings, MF_STRING, IDM_SAVE_CONFIG, "Guardar Configuracion");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuSettings, "Opciones");

    // 5. Menu Herramientas
    HMENU hMenuTools = CreatePopupMenu();
    AppendMenuA(hMenuTools, MF_STRING, IDM_TAKE_SCREENSHOT, "Tomar Captura (Screenshot)");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuTools, "Herramientas");

    SetMenu(g_hwnd, g_hMenu);
    DrawMenuBar(g_hwnd);

    // Subclass the Window Procedure
    g_oldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);

    updateMenuChecks();
}

void Application::updateMenuChecks() {
    if (!g_hMenu) return;

    // Devices
    for (size_t i = 0; i < devices.size(); i++) {
        CheckMenuItem(g_hMenu, IDM_DEVICE_0 + i, MF_BYCOMMAND | (deviceIndex == (int)i ? MF_CHECKED : MF_UNCHECKED));
    }

    // Input
    CheckMenuItem(g_hMenu, IDM_RES_1080, MF_BYCOMMAND | (capWidth == 1920 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RES_720,  MF_BYCOMMAND | (capWidth == 1280 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_FPS_60,   MF_BYCOMMAND | (capFps == 60 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_FPS_30,   MF_BYCOMMAND | (capFps == 30 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_MJPG_TOGGLE, MF_BYCOMMAND | (forceMjpg ? MF_CHECKED : MF_UNCHECKED));

    // IA
    CheckMenuItem(g_hMenu, IDM_DENOISE_TOGGLE, MF_BYCOMMAND | (enableDenoise ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_DENOISE_00, MF_BYCOMMAND | (denoiseStrength == 0.0f ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_DENOISE_05, MF_BYCOMMAND | (denoiseStrength == 0.5f ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_DENOISE_10, MF_BYCOMMAND | (denoiseStrength == 1.0f ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem(g_hMenu, IDM_AA_TOGGLE, MF_BYCOMMAND | (enableAA ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem(g_hMenu, IDM_AI_TOGGLE, MF_BYCOMMAND | (enableAI ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_TARGET_1080, MF_BYCOMMAND | (srWidth == 1920 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_TARGET_1440, MF_BYCOMMAND | (srWidth == 2560 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_TARGET_2160, MF_BYCOMMAND | (srWidth == 3840 ? MF_CHECKED : MF_UNCHECKED));
}

void Application::handleWin32Command(int menuId) {
    // 1. Devices
    if (menuId >= IDM_DEVICE_0 && menuId < IDM_DEVICE_0 + 10) {
        int id = menuId - IDM_DEVICE_0;
        if (id < (int)devices.size()) {
            deviceIndex = id;
            pendingCaptureRestart = true;
        }
    }
    // 2. Input
    else if (menuId == IDM_RES_1080) { capWidth = 1920; capHeight = 1080; pendingCaptureRestart = true; }
    else if (menuId == IDM_RES_720)  { capWidth = 1280; capHeight = 720; pendingCaptureRestart = true; }
    else if (menuId == IDM_FPS_60)   { capFps = 60; pendingCaptureRestart = true; }
    else if (menuId == IDM_FPS_30)   { capFps = 30; pendingCaptureRestart = true; }
    else if (menuId == IDM_MJPG_TOGGLE) { forceMjpg = !forceMjpg; pendingCaptureRestart = true; }
    // 3. IA Denoise
    else if (menuId == IDM_DENOISE_TOGGLE) {
        if (!enableDenoise) { pendingDenoiseInit = true; } else { enableDenoise = false; }
    }
    else if (menuId == IDM_DENOISE_00) { denoiseStrength = 0.0f; enableDenoise = false; videoProcessor.releaseDenoiser(); pendingDenoiseInit = true; }
    else if (menuId == IDM_DENOISE_05) { denoiseStrength = 0.5f; enableDenoise = false; videoProcessor.releaseDenoiser(); pendingDenoiseInit = true; }
    else if (menuId == IDM_DENOISE_10) { denoiseStrength = 1.0f; enableDenoise = false; videoProcessor.releaseDenoiser(); pendingDenoiseInit = true; }
    // 4. AA & SR
    else if (menuId == IDM_AA_TOGGLE) { enableAA = !enableAA; }
    else if (menuId == IDM_AI_TOGGLE) {
        if (!enableAI) { pendingAIInit = true; } else { enableAI = false; }
    }
    else if (menuId == IDM_TARGET_1080) { srWidth = 1920; srHeight = 1080; enableAI = false; videoProcessor.releaseUpscaler(); }
    else if (menuId == IDM_TARGET_1440) { srWidth = 2560; srHeight = 1440; enableAI = false; videoProcessor.releaseUpscaler(); }
    else if (menuId == IDM_TARGET_2160) { srWidth = 3840; srHeight = 2160; enableAI = false; videoProcessor.releaseUpscaler(); }
    // 5. Config
    else if (menuId == IDM_SAVE_CONFIG) {
        AppConfig cfg;
        cfg.deviceIndex = deviceIndex;
        cfg.capWidth = capWidth;
        cfg.capHeight = capHeight;
        cfg.capFps = capFps;
        cfg.srWidth = srWidth;
        cfg.srHeight = srHeight;
        cfg.enableDenoise = enableDenoise;
        cfg.enableAI = enableAI;
        cfg.enableAA = enableAA;
        cfg.forceMjpg = forceMjpg;
        ConfigManager::saveConfig("config.ini", cfg);
        std::cout << "Configuracion guardada en config.ini via Menu Nativo." << std::endl;
    }
    // 6. Herramientas
    else if (menuId == IDM_TAKE_SCREENSHOT) {
        pendingScreenshot = true;
    }
    
    updateMenuChecks();
}


void Application::run() {
    isRunning = true;
    cv::Mat frame, processedFrame;

    // Inicializamos primero la venta y obtenemos el primer context para poder instalar la barra de Win32
    // Enviamos un frame negro temporal para que OpenCV cree y registre el HWND internamente 
    appWindow.show(cv::Mat(720, 1280, CV_8UC3, cv::Scalar(0, 0, 0)));
    cv::waitKey(10); // Bombear primer evento

    setupNativeMenu();

    bool captureActive = false;
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight, capFps, forceMjpg);
        if (captureActive) {
            std::string devName = devices[deviceIndex].name;
            std::wstring wideTitle = L"\u00D1oquis Viwer - " + std::wstring(devName.begin(), devName.end());
            SetWindowTextW(g_hwnd, wideTitle.c_str());
            audioManager.start();
            if (enableAI) pendingAIInit = true;
            if (enableDenoise) pendingDenoiseInit = true;
        }
    }

    std::cout << "Controles: [ESC] Zen Mode | Selecciona Opciones en la Barra Win32 Superior." << std::endl;

    while (isRunning) {
        if (pendingCaptureRestart) {
            std::cout << "Reiniciando captura de video..." << std::endl;
            captureManager.release();
            audioManager.stop();
            bool oldAI = enableAI;
            bool oldDenoise = enableDenoise;
            enableAI = false;
            enableDenoise = false;
            videoProcessor.releaseUpscaler();
            videoProcessor.releaseDenoiser();
            if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
                captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight, capFps, forceMjpg);
                if (captureActive) {
                    std::string devName = devices[deviceIndex].name;
                    std::wstring wideTitle = L"\u00D1oquis Viwer - " + std::wstring(devName.begin(), devName.end());
                    SetWindowTextW(g_hwnd, wideTitle.c_str());
                    audioManager.start();
                    if (oldAI) pendingAIInit = true;
                    if (oldDenoise) pendingDenoiseInit = true;
                }
            }
            pendingCaptureRestart = false;
            updateMenuChecks();
        }

        if (pendingAIInit) {
            std::cout << "Inicializando Super Resolution..." << std::endl;
            bool ready = videoProcessor.isUpscalerReady();
            if (!ready) ready = videoProcessor.initUpscaler(capWidth, capHeight, srWidth, srHeight, GPUUpscaler::kModeMjpegDefault);
            enableAI = ready;
            pendingAIInit = false;
            updateMenuChecks();
        }

        if (pendingDenoiseInit) {
            std::cout << "Inicializando Denoise..." << std::endl;
            bool ready = videoProcessor.isDenoiserReady();
            if (!ready) ready = videoProcessor.initDenoiser(capWidth, capHeight, denoiseStrength);
            enableDenoise = ready;
            pendingDenoiseInit = false;
            updateMenuChecks();
        }

        if (captureActive) {
            if (!captureManager.getNextFrame(frame)) {
                frame = cv::Mat(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));
            }
        } else {
            frame = cv::Mat(720, 1280, CV_8UC3, cv::Scalar(20, 20, 20));
        }

        auto start = std::chrono::high_resolution_clock::now();
        
        videoProcessor.processFrame(frame, processedFrame, enableAI && captureActive, enableDenoise && captureActive);
        
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        static std::ofstream profilerLog("capturadora_rendimiento.log");
        if ((enableAI || enableDenoise) && captureActive) {
            profilerLog << "[Profiler] Pipeline IA tomo: " << elapsedMs 
                        << " ms | Denoise: " << (enableDenoise ? "ON" : "OFF") 
                        << " | VSR: " << (enableAI ? "ON" : "OFF") << std::endl;
        }

        cv::Mat displayFrame = processedFrame;
        
        if (enableAA) {
            if (displayFrame.cols != srWidth || displayFrame.rows != srHeight) {
                cv::Mat aaFrame;
                cv::resize(displayFrame, aaFrame, cv::Size(srWidth, srHeight), 0, 0, cv::INTER_LANCZOS4);
                displayFrame = aaFrame;
            }
        }

        if (pendingScreenshot) {
            CreateDirectoryA("capturas", NULL);
            auto t = std::time(nullptr);
            auto tm = *std::localtime(&t);
            char buf[64];
            std::strftime(buf, sizeof(buf), "capturas\\captura_%Y%m%d_%H%M%S.png", &tm);
            cv::imwrite(buf, displayFrame);
            std::cout << "[App] Captura guardada en: " << buf << std::endl;
            pendingScreenshot = false;
        }

        if (appWindow.isWindowFullscreen()) {
            if (GetMenu(g_hwnd) != NULL) SetMenu(g_hwnd, NULL);
            int w = displayFrame.cols;
            int h = displayFrame.rows;
            int targetH = (w * 10) / 16;
            if (targetH > h) {
                int pad = (targetH - h) / 2;
                cv::copyMakeBorder(displayFrame, displayFrame, pad, pad, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
            }
        } else {
            if (GetMenu(g_hwnd) == NULL) {
                SetMenu(g_hwnd, g_hMenu);
                DrawMenuBar(g_hwnd);
            }
        }

        appWindow.show(displayFrame);
        handleInput(captureActive);
        
        // Manejo del aspa [X] de Win32 en vez de solo presionar ESC
        if (cv::getWindowProperty(appWindow.getName(), cv::WND_PROP_VISIBLE) < 1) {
            isRunning = false;
            break;
        }
    }
    
    // Unhook before exit to prevent crash
    if (g_oldWndProc && g_hwnd) {
        SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
    }

    std::cout << "Recursos liberados. Programa terminado." << std::endl;
}

void Application::handleInput(bool& captureActive) {
    char key = (char)cv::waitKey(1); 
    if (key == 27) { appWindow.toggleFullscreen(); }
    else if (key == 'm' || key == 'M') { /* Desactivado el overlay transparente */ }
    else if (key == 'q' || key == 'Q') { isRunning = false; }
}


