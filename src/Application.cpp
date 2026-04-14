#include "Application.hpp"
#include <iostream>

Application::Application(int deviceIndex) 
    : deviceIndex(deviceIndex), isRunning(false), enableAI(false), enableDenoise(false), showMenu(true), appWindow("AI-Link Capture") {
    devices = CaptureManager::getAvailableDevices();
}

Application::~Application() {
    captureManager.release();
}

void Application::run() {
    isRunning = true;
    cv::Mat frame, processedFrame;

    // Intentamos conectar el dispositivo SOLO si hay uno pre-seleccionado
    bool captureActive = false;
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight);
        if (captureActive) {
            audioManager.start();
        }
    }

    std::cout << "Controles: [ESC] Zen Mode | [M] Menu | [F] IA | [R] Input Res | [S] Output Res | [Q] Salir" << std::endl;

    while (isRunning) {
        if (captureActive) {
            // Ingesta y Decodificación
            if (!captureManager.getNextFrame(frame)) {
                // Si el frame falla, dibujamos negro en vez de crashear
                frame = cv::Mat(1080, 1920, CV_8UC3, cv::Scalar(0, 0, 0));
            }
        } else {
            // Sin captura activa: pantalla negra con instrucciones
            frame = cv::Mat(720, 1280, CV_8UC3, cv::Scalar(20, 20, 20));
            showMenu = true; // Forzar menú visible
        }

        // Post-proceso: Denoise y/o Super Resolution
        videoProcessor.processFrame(frame, processedFrame, enableAI && captureActive, enableDenoise && captureActive);

        // HUD OSD - Menu de selección
        if (showMenu) {
            drawMenu(processedFrame);
        }

        // Letterboxing manual para mantener 16:9 en pantallas 16:10 si estamos en pantalla completa
        cv::Mat displayFrame = processedFrame;
        if (appWindow.isWindowFullscreen()) {
            int w = displayFrame.cols;
            int h = displayFrame.rows;
            int targetH = (w * 10) / 16; // Calcular altura para ratio 16:10
            if (targetH > h) {
                int pad = (targetH - h) / 2;
                cv::copyMakeBorder(displayFrame, displayFrame, pad, pad, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
            }
        }

        // Renderizado
        appWindow.show(displayFrame);

        // Input - pasamos referencia a captureActive para poder actualizarla
        handleInput(captureActive);
    }

    std::cout << "Recursos liberados. Programa terminado." << std::endl;
}


void Application::handleInput(bool& captureActive) {
    // Lectura no bloqueante de 1ms
    char key = (char)cv::waitKey(1); 
    
    if (key == 27) { // Tecla ESC
        appWindow.toggleFullscreen();
    }
    else if (key == 'm' || key == 'M') { // Menu de camaras
        showMenu = !showMenu;
    }
    else if (key == 'd' || key == 'D') { // Tecla D - Toggle Denoise
        if (captureActive) {
            if (!enableDenoise) {
                bool denoiseReady = videoProcessor.isDenoiserReady();
                if (!denoiseReady) {
                    std::cout << "Inicializando Denoise..." << std::endl;
                    denoiseReady = videoProcessor.initDenoiser(capWidth, capHeight, 1.0f);
                }

                if (!denoiseReady) {
                    enableDenoise = false;
                    std::cout << "Denoise: ERROR (no se pudo inicializar el modelo IA)" << std::endl;
                    return;
                }
            }

            enableDenoise = !enableDenoise;
            std::cout << "Denoise: " << (enableDenoise ? "ON (Reduccion de Ruido IA)" : "OFF") << std::endl;
        }
    }
    else if (key == 'f' || key == 'F') { // Tecla F - Toggle Super Resolution
        if (captureActive) {
            if (!enableAI) {
                bool upscalerReady = videoProcessor.isUpscalerReady();
                if (!upscalerReady) {
                    // Primera vez: inicializar el upscaler 1080p -> 2K
                    std::cout << "Inicializando Super Resolution..." << std::endl;
                    upscalerReady = videoProcessor.initUpscaler(capWidth, capHeight, srWidth, srHeight, 1);
                }

                if (!upscalerReady) {
                    enableAI = false;
                    std::cout << "IA: ERROR (no se pudo inicializar Super Resolution)" << std::endl;
                    return;
                }
            }

            enableAI = !enableAI;
            std::cout << "IA: " << (enableAI ? "ON (Super Resolution)" : "OFF (Pass-through)") << std::endl;
        }
    }
    else if (key == 'r' || key == 'R') { // Select Input Resolution
        capWidth = (capWidth == 1920) ? 1280 : 1920;
        capHeight = (capHeight == 1080) ? 720 : 1080;
        if (captureActive) {
            std::cout << "Cambiando captura a " << capWidth << "x" << capHeight << "..." << std::endl;
            captureManager.release();
            audioManager.stop();
            enableAI = false;
            enableDenoise = false;
            videoProcessor.releaseUpscaler();
            videoProcessor.releaseDenoiser();
            captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight);
            if (captureActive) audioManager.start();
        }
    }
    else if (key == 's' || key == 'S') { // Select Output Resolution (DLSS Target)
        if (srWidth == 2560) { srWidth = 1920; srHeight = 1080; } // 1440p -> 1080p
        else if (srWidth == 1920) { srWidth = 3840; srHeight = 2160; } // 1080p -> 4K
        else { srWidth = 2560; srHeight = 1440; } // 4K -> 1440p
        
        enableAI = false;
        enableDenoise = false;
        videoProcessor.releaseUpscaler();
        videoProcessor.releaseDenoiser();
        std::cout << "Objetivo IA seleccionado: " << srWidth << "x" << srHeight << std::endl;
    }
    else if (key == 'q' || key == 'Q') { // Cierre Seguro
        std::cout << "Iniciando cierre seguro..." << std::endl;
        isRunning = false;
    }
    else if (key >= '0' && key <= '9') { // Selección de cámara (0-9)
        if (showMenu) {
            int newIndex = key - '0';
            switchDevice(newIndex, captureActive);
            showMenu = false; // Cerrar menu al seleccionar
        }
    }
}

void Application::switchDevice(int menuIndex, bool& captureActive) {
    if (menuIndex >= 0 && menuIndex < (int)devices.size()) {
        if (menuIndex != deviceIndex || !captureActive) {
            int hwIdx = devices[menuIndex].hwIndex;
            std::cout << "Conectando dispositivo HW[" << hwIdx << "]: " << devices[menuIndex].name << std::endl;
            captureManager.release();
            audioManager.stop();
            enableAI = false;
            enableDenoise = false;
            deviceIndex = menuIndex;
            videoProcessor.releaseUpscaler();
            videoProcessor.releaseDenoiser();
            captureActive = captureManager.initialize(hwIdx, capWidth, capHeight);
            if (captureActive) {
                audioManager.start();
                std::cout << "Captura activa. [F] Activar Super Resolution | [ESC] Zen | [M] Menu | [Q] Salir" << std::endl;
            }
        }
    }
}

void Application::drawMenu(cv::Mat& frame) {
    // Dibujar un cuadro semitransparente oscuro
    cv::Mat overlay;
    frame.copyTo(overlay);
    int menuHeight = 80 + 40 * devices.size();
    cv::rectangle(overlay, cv::Point(20, 20), cv::Point(600, menuHeight), cv::Scalar(0, 0, 0), cv::FILLED);
    cv::addWeighted(overlay, 0.7, frame, 0.3, 0, frame);

    // Titulo del menu
    cv::putText(frame, "Selecciona una capturadora (Teclas 0-9):", cv::Point(40, 50), 
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);

    if (devices.empty()) {
        cv::putText(frame, "No se encontraron dispositivos.", cv::Point(40, 90), 
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
        return;
    }

    // Listar las capturadoras
    for (size_t i = 0; i < devices.size(); i++) {
        cv::Scalar color = ((int)i == deviceIndex) ? cv::Scalar(0, 255, 0) : cv::Scalar(200, 200, 200);
        std::string text = "[" + std::to_string(i) + "] " + devices[i].name + (((int)i == deviceIndex) ? " (Activa)" : "");
        cv::putText(frame, text, cv::Point(40, 90 + (int)i * 40), 
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 2);
    }

    int baseLine = 90 + devices.size() * 40 + 20;
    cv::putText(frame, "---------------------------------", cv::Point(40, baseLine), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
    cv::putText(frame, "[R] Resolucion USB: " + std::to_string(capWidth) + "x" + std::to_string(capHeight) + " (R para cambiar)", cv::Point(40, baseLine + 30), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);
    cv::putText(frame, "[D] Denoise IA: " + std::string(enableDenoise ? "ON" : "OFF"), cv::Point(40, baseLine + 60), cv::FONT_HERSHEY_SIMPLEX, 0.6, enableDenoise ? cv::Scalar(0, 200, 255) : cv::Scalar(180, 180, 180), 2);
    cv::putText(frame, "[F] Super Res IA: " + std::string(enableAI ? "ON (" + std::to_string(srWidth) + "x" + std::to_string(srHeight) + ")" : "OFF"), cv::Point(40, baseLine + 90), cv::FONT_HERSHEY_SIMPLEX, 0.6, enableAI ? cv::Scalar(0, 255, 100) : cv::Scalar(180, 180, 180), 2);
    cv::putText(frame, "[S] Objetivo IA: " + std::to_string(srWidth) + "x" + std::to_string(srHeight) + " (S para cambiar)", cv::Point(40, baseLine + 120), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 165, 0), 2);
}
