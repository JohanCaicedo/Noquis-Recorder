#include "Application.hpp"
#include <iostream>

// Instancia global para el callback del raton
Application* g_appInstance = nullptr;

void Application::onMouse(int event, int x, int y, int flags, void* userdata) {
    if (g_appInstance && event == cv::EVENT_LBUTTONDOWN) {
        g_appInstance->handleMouse(event, x, y, flags);
    }
}

Application::Application(int deviceIndex) 
    : isRunning(false), enableAA(false), enableAI(false), enableDenoise(false), showMenu(true), appWindow("AI-Link Capture"), pendingCaptureRestart(false) {
    
    devices = CaptureManager::getAvailableDevices();

    // Cargar config si existe, sino usar valores default de AppConfig y el deviceIndex por consola
    AppConfig cfg = ConfigManager::loadConfig("config.ini");
    if (cfg.deviceIndex != -1) {
        // Usar la config del archivo local
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
    } else {
        this->deviceIndex = deviceIndex;
    }

    g_appInstance = this;
}

Application::~Application() {
    captureManager.release();
    g_appInstance = nullptr;
}

void Application::run() {
    isRunning = true;
    cv::Mat frame, processedFrame;

    appWindow.setMouseCallback(Application::onMouse, nullptr);

    bool captureActive = false;
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight, capFps);
        if (captureActive) {
            audioManager.start();
        }
    }

    std::cout << "Controles: [ESC] Zen Mode | [M] Menu | [F] IA | [R] Input Res | [S] Output Res | [Q] Salir" << std::endl;

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
                captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight, capFps);
                if (captureActive) {
                    audioManager.start();
                    enableAI = oldAI;
                    enableDenoise = oldDenoise;
                    // Reactivar modelos si estaban prendidos antes (esto forzara que se re-inicien en el frame loop o los desactivamos definitivamente para que el usuario los prenda)
                    // Por simplicidad de manejo de memoria, mejor los dejo en false para que el usuario de click de nuevo o el primer frame los cargue via logic si hacemos auto-hot load. 
                    // Ya que la logica de inicializacion esta en 'handleInput/Mouse'. Para evitar crash, mejor en false.
                    enableAI = false; 
                    enableDenoise = false;
                }
            }
            pendingCaptureRestart = false;
        }

        if (captureActive) {
            if (!captureManager.getNextFrame(frame)) {
                frame = cv::Mat(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));
            }
        } else {
            frame = cv::Mat(720, 1280, CV_8UC3, cv::Scalar(20, 20, 20));
            showMenu = true;
        }

        videoProcessor.processFrame(frame, processedFrame, enableAI && captureActive, enableDenoise && captureActive);

        if (showMenu) {
            drawMenu(processedFrame);
        }

        cv::Mat displayFrame = processedFrame;
        
        // Capa de Anti-Aliasing (Lanczos4) independiente
        if (enableAA) {
            if (displayFrame.cols != srWidth || displayFrame.rows != srHeight) {
                cv::Mat aaFrame;
                cv::resize(displayFrame, aaFrame, cv::Size(srWidth, srHeight), 0, 0, cv::INTER_LANCZOS4);
                displayFrame = aaFrame;
            }
        }

        if (appWindow.isWindowFullscreen()) {
            int w = displayFrame.cols;
            int h = displayFrame.rows;
            int targetH = (w * 10) / 16;
            if (targetH > h) {
                int pad = (targetH - h) / 2;
                cv::copyMakeBorder(displayFrame, displayFrame, pad, pad, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
            }
        }

        appWindow.show(displayFrame);
        handleInput(captureActive);
    }

    std::cout << "Recursos liberados. Programa terminado." << std::endl;
}

void Application::handleInput(bool& captureActive) {
    char key = (char)cv::waitKey(1); 
    
    if (key == 27) { appWindow.toggleFullscreen(); }
    else if (key == 'm' || key == 'M') { showMenu = !showMenu; }
    else if (key == 'q' || key == 'Q') { isRunning = false; }
}

void Application::handleMouse(int event, int x, int y, int flags) {
    if (!showMenu) return;
    cv::Point pt(x, y);

    for (size_t i = 0; i < btnsDevices.size(); i++) {
        if (btnsDevices[i].contains(pt)) {
            bool dummy = true; // pasamos una ref temporal
            pendingCaptureRestart = true;
            deviceIndex = (int)i;
        }
    }
    
    if (btnResToggle.contains(pt)) {
        capWidth = (capWidth == 1920) ? 1280 : 1920;
        capHeight = (capHeight == 1080) ? 720 : 1080;
        pendingCaptureRestart = true;
    }
    
    if (btnFpsToggle.contains(pt)) {
        capFps = (capFps == 60) ? 30 : 60;
        pendingCaptureRestart = true;
    }

    if (btnAIToggle.contains(pt)) {
        if (!enableAI) {
            bool ready = videoProcessor.isUpscalerReady();
            if (!ready) ready = videoProcessor.initUpscaler(capWidth, capHeight, srWidth, srHeight, GPUUpscaler::kModeMjpegDefault);
            if (ready) enableAI = true;
        } else {
            enableAI = false;
        }
    }

    if (btnAAToggle.contains(pt)) {
        enableAA = !enableAA;
    }

    if (btnOutToggle.contains(pt)) {
        if (srWidth == 2560) { srWidth = 1920; srHeight = 1080; } 
        else if (srWidth == 1920) { srWidth = 3840; srHeight = 2160; } 
        else { srWidth = 2560; srHeight = 1440; }
        
        enableAI = false; // reset ai
        videoProcessor.releaseUpscaler();
    }

    if (btnDenoiseToggle.contains(pt)) {
        if (!enableDenoise) {
            bool ready = videoProcessor.isDenoiserReady();
            if (!ready) ready = videoProcessor.initDenoiser(capWidth, capHeight, denoiseStrength);
            if (ready) enableDenoise = true;
        } else {
            enableDenoise = false;
        }
    }

    if (btnDenoiseStrength.contains(pt)) {
        if (denoiseStrength == 0.0f) denoiseStrength = 0.5f;
        else if (denoiseStrength == 0.5f) denoiseStrength = 1.0f;
        else denoiseStrength = 0.0f;
        
        enableDenoise = false; // reset denoise
        videoProcessor.releaseDenoiser();
    }

    if (btnSave.contains(pt)) {
        AppConfig cfg;
        cfg.deviceIndex = deviceIndex;
        cfg.capWidth = capWidth;
        cfg.capHeight = capHeight;
        cfg.capFps = capFps;
        cfg.srWidth = srWidth;
        cfg.srHeight = srHeight;
        cfg.denoiseStrength = denoiseStrength;
        cfg.enableDenoise = enableDenoise;
        cfg.enableAI = enableAI;
        cfg.enableAA = enableAA;
        ConfigManager::saveConfig("config.ini", cfg);
        std::cout << "Configuracion guardada en config.ini." << std::endl;
    }
}

void Application::switchDevice(int menuIndex, bool& captureActive) {
    if (menuIndex >= 0 && menuIndex < (int)devices.size()) {
        if (menuIndex != deviceIndex || !captureActive) {
            deviceIndex = menuIndex;
            pendingCaptureRestart = true;
        }
    }
}

void Application::drawMenu(cv::Mat& frame) {
    cv::Mat overlay;
    frame.copyTo(overlay);
    
    // Panel central grande
    int w = 700;
    int h = 450;
    int rx = 40;
    int ry = 40;
    cv::rectangle(overlay, cv::Rect(rx, ry, w, h), cv::Scalar(15, 15, 15), cv::FILLED);
    cv::addWeighted(overlay, 0.85, frame, 0.15, 0, frame);
    cv::rectangle(frame, cv::Rect(rx, ry, w, h), cv::Scalar(200, 200, 200), 2); // Borde

    cv::putText(frame, "--- AI-LINK CAPTURE : SETTINGS (Clickable) ---", cv::Point(rx + 20, ry + 40), 
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);

    int textX = rx + 30;
    int y = ry + 80;

    // Devices
    btnsDevices.clear();
    cv::putText(frame, "DISPOSITIVO DE VIDEO:", cv::Point(textX, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 200), 1);
    for (size_t i = 0; i < devices.size(); i++) {
        std::string label = devices[i].name;
        cv::Scalar color = ((int)i == deviceIndex) ? cv::Scalar(0, 255, 0) : cv::Scalar(100, 100, 100);
        cv::Rect btnRect(textX + 220, y - 20, 300, 30);
        btnsDevices.push_back(btnRect);
        cv::rectangle(frame, btnRect, color, ((int)i == deviceIndex) ? cv::FILLED : 1);
        cv::putText(frame, label, cv::Point(btnRect.x + 10, btnRect.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, ((int)i == deviceIndex) ? cv::Scalar(0,0,0) : cv::Scalar(255,255,255), 1);
        y += 40;
    }
    y += 20;

    // Res / FPS
    cv::putText(frame, "ENTRADA (Hardware):", cv::Point(textX, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 200), 1);
    btnResToggle = cv::Rect(textX + 220, y - 20, 150, 30);
    cv::rectangle(frame, btnResToggle, cv::Scalar(255, 255, 100), 1);
    cv::putText(frame, std::to_string(capWidth) + "x" + std::to_string(capHeight), cv::Point(btnResToggle.x + 20, btnResToggle.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 100), 1);
    
    btnFpsToggle = cv::Rect(textX + 380, y - 20, 100, 30);
    cv::rectangle(frame, btnFpsToggle, cv::Scalar(255, 150, 150), 1);
    cv::putText(frame, std::to_string(capFps) + " FPS", cv::Point(btnFpsToggle.x + 20, btnFpsToggle.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 150, 150), 1);
    y += 50;

    // Denoise
    cv::putText(frame, "FILTRO DENOISE IA:", cv::Point(textX, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 200), 1);
    btnDenoiseToggle = cv::Rect(textX + 220, y - 20, 100, 30);
    cv::rectangle(frame, btnDenoiseToggle, enableDenoise ? cv::Scalar(0, 255, 0) : cv::Scalar(100, 100, 100), enableDenoise ? cv::FILLED : 1);
    cv::putText(frame, enableDenoise ? "ENCENDIDO" : "APAGADO", cv::Point(btnDenoiseToggle.x + 10, btnDenoiseToggle.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, enableDenoise ? cv::Scalar(0,0,0) : cv::Scalar(255,255,255), 1);

    btnDenoiseStrength = cv::Rect(textX + 330, y - 20, 150, 30);
    cv::rectangle(frame, btnDenoiseStrength, cv::Scalar(200, 200, 200), 1);
    std::string strStr = "Fuerza: " + std::to_string(denoiseStrength).substr(0,3);
    cv::putText(frame, strStr, cv::Point(btnDenoiseStrength.x + 20, btnDenoiseStrength.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(200, 200, 200), 1);
    y += 50;

    // AI Super Res Target
    cv::putText(frame, "NVIDIA SUPER RES:", cv::Point(textX, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 200), 1);
    btnAIToggle = cv::Rect(textX + 220, y - 20, 100, 30);
    cv::rectangle(frame, btnAIToggle, enableAI ? cv::Scalar(0, 255, 0) : cv::Scalar(100, 100, 100), enableAI ? cv::FILLED : 1);
    cv::putText(frame, enableAI ? "ENCENDIDO" : "APAGADO", cv::Point(btnAIToggle.x + 10, btnAIToggle.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, enableAI ? cv::Scalar(0,0,0) : cv::Scalar(255,255,255), 1);

    cv::putText(frame, "AA LANCZOS4:", cv::Point(textX + 340, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 200), 1);
    btnAAToggle = cv::Rect(textX + 480, y - 20, 100, 30);
    cv::rectangle(frame, btnAAToggle, enableAA ? cv::Scalar(0, 255, 0) : cv::Scalar(100, 100, 100), enableAA ? cv::FILLED : 1);
    cv::putText(frame, enableAA ? "ENCENDIDO" : "APAGADO", cv::Point(btnAAToggle.x + 10, btnAAToggle.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.5, enableAA ? cv::Scalar(0,0,0) : cv::Scalar(255,255,255), 1);
    y += 50;

    cv::putText(frame, "TARGET ALTA RES:", cv::Point(textX, y), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 200), 1);
    btnOutToggle = cv::Rect(textX + 220, y - 20, 150, 30);
    cv::rectangle(frame, btnOutToggle, cv::Scalar(100, 255, 255), 1);
    cv::putText(frame, "Target: " + std::to_string(srWidth)+"x"+std::to_string(srHeight), cv::Point(btnOutToggle.x + 10, btnOutToggle.y + 20), cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(100, 255, 255), 1);
    y += 60;

    // Guarda info
    btnSave = cv::Rect(textX, y - 20, 200, 40);
    cv::rectangle(frame, btnSave, cv::Scalar(0, 150, 255), cv::FILLED);
    cv::putText(frame, "GUARDAR CONFIG", cv::Point(btnSave.x + 30, btnSave.y + 25), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 2);
    
    cv::putText(frame, "(Presiona [M] para cerrar este menu)", cv::Point(textX + 220, y), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(150, 150, 150), 1);
}
