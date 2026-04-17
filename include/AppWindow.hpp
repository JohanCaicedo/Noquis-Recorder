#pragma once
#include <opencv2/opencv.hpp>
#include <string>

class AppWindow {
public:
    AppWindow(const std::string& windowName);
    ~AppWindow();

    // Muestra el frame en la ventana
    void show(const cv::Mat& frame);
    
    // Alterna entre pantalla completa y modo ventana
    void toggleFullscreen();
    bool isWindowFullscreen() const { return isFullscreen; }
    
    // Devuelve el nombre identificador de la ventana de OpenCV
    const std::string& getName() const { return name; }

    // Interceptar eventos de ratón
    void setMouseCallback(cv::MouseCallback onMouse, void* userdata);

private:
    std::string name;
    bool isFullscreen;
};
