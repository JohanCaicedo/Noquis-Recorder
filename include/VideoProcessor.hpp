#pragma once
#include <opencv2/opencv.hpp>
#include "GPUUpscaler.hpp"

class VideoProcessor {
public:
    VideoProcessor();
    ~VideoProcessor();

    // Inicializa el upscaler GPU (720p -> 1440p)
    bool initUpscaler(int srcW, int srcH, int dstW, int dstH, int quality = 1);

    // Procesa el frame: si enableAI, escala con GPU; sino, pass-through
    void processFrame(const cv::Mat& inputFrame, cv::Mat& outputFrame, bool enableAI);

    // Destruye el upscaler para cambiar resoluciones
    void releaseUpscaler();

    // Verifica si el upscaler esta listo
    bool isUpscalerReady() const;

private:
    GPUUpscaler upscaler;
};

