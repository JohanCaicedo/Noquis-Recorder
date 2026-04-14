#include "AppWindow.hpp"

AppWindow::AppWindow(const std::string& windowName) : name(windowName), isFullscreen(false) {
    // Flags para crear una ventana limpia. WINDOW_KEEPRATIO evita que se estire a 16:10
    cv::namedWindow(name, cv::WINDOW_NORMAL | cv::WINDOW_KEEPRATIO | cv::WINDOW_GUI_EXPANDED);
}

AppWindow::~AppWindow() {
    cv::destroyWindow(name);
}

void AppWindow::show(const cv::Mat& frame) {
    cv::imshow(name, frame);
}

void AppWindow::toggleFullscreen() {
    isFullscreen = !isFullscreen;
    cv::setWindowProperty(name, cv::WND_PROP_FULLSCREEN, 
        isFullscreen ? cv::WINDOW_FULLSCREEN : cv::WINDOW_NORMAL);
}
