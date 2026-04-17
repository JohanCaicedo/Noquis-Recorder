#pragma once
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <utility>

struct DeviceInfo {
    int hwIndex;        // Indice real del hardware (0-9)
    std::string name;   // Nombre legible
};

class CaptureManager {
public:
    CaptureManager();
    ~CaptureManager();

    // Obtener la lista nativa de dispositivos DirectShow
    static std::vector<DeviceInfo> getAvailableDevices();

    // Inicia la captura desde un dispositivo especificado con una resolución dada.
    bool initialize(int deviceIndex, int width = 1920, int height = 1080, int fps = 60, bool useMjpeg = true);
    
    // Obtiene el siguiente frame. Retorna false si el frame está vacío.
    bool getNextFrame(cv::Mat& outFrame);
    
    // Libera los recursos
    void release();

private:
    cv::VideoCapture cap;
};

