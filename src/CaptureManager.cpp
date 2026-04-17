#include "CaptureManager.hpp"
#include <opencv2/core/utils/logger.hpp>
#include <iostream>

CaptureManager::CaptureManager() {}

CaptureManager::~CaptureManager() {
    release();
}

bool CaptureManager::initialize(int deviceIndex, int width, int height, int fps, bool useMjpeg) {
    // Ahora que recompilamos OpenCV con MSMF, esto funcionara perfecto.
    // Media Foundation respeta el FOURCC y no necesita Popups.
    cap.open(deviceIndex, cv::CAP_MSMF);
    
    if (!cap.isOpened()) {
        std::cerr << "Error: No se pudo abrir la capturadora USB en el indice " << deviceIndex << "." << std::endl;
        return false;
    }

    // ============================================================
    // CONFIGURACION OPTIMIZADA PARA USB
    // ============================================================
    
    // Forzamos MJPEG si el usuario lo pide o por defecto para USB 2.0
    if (useMjpeg) {
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    }
    
    cap.set(cv::CAP_PROP_FRAME_WIDTH, width);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, height);
    cap.set(cv::CAP_PROP_FPS, fps);

    // 4. ZERO-BUFFER: Forzar buffer a 1 frame.
    //    Evita que OpenCV encole frames antiguos (causa de lag).
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

    // Verificar la configuracion real del hardware
    double realWidth = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    double realHeight = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    double realFPS = cap.get(cv::CAP_PROP_FPS);
    std::cout << "Dispositivo " << deviceIndex 
              << " configurado: " << realWidth << "x" << realHeight 
              << " @ " << realFPS << " FPS (" << (useMjpeg ? "MJPEG" : "Uncompressed") << ")" << std::endl;

    return true;
}

bool CaptureManager::getNextFrame(cv::Mat& outFrame) {
    // grab() + retrieve() es mas rapido que >> porque grab()
    // no decodifica hasta que retrieve() lo necesite.
    // Esto reduce la latencia en ~1-2ms por frame.
    if (!cap.grab()) return false;
    return cap.retrieve(outFrame);
}

void CaptureManager::release() {
    if (cap.isOpened()) {
        cap.release();
    }
}

std::vector<DeviceInfo> CaptureManager::getAvailableDevices() {
    std::vector<DeviceInfo> devices;

    std::cout << "Escaneando dispositivos de video..." << std::endl;

    const auto previousLogLevel = cv::utils::logging::getLogLevel();
    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);
    
    // Probamos indices 0-4 con MSMF
    for (int i = 0; i < 5; i++) {
        cv::VideoCapture testCap;
        if (testCap.open(i, cv::CAP_MSMF)) {
            DeviceInfo info;
            info.hwIndex = i;
            info.name = "Dispositivo de Video [" + std::to_string(i) + "]";
            devices.push_back(info);
            testCap.release();
        }
    }

    cv::utils::logging::setLogLevel(previousLogLevel);
    
    std::cout << "Encontrados " << devices.size() << " dispositivo(s)." << std::endl;
    return devices;
}
