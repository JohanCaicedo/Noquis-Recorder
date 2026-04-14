#pragma once
#include <opencv2/opencv.hpp>
#include "GPUUpscaler.hpp"
#include "GPUDenoiser.hpp"

class VideoProcessor {
public:
    VideoProcessor();
    ~VideoProcessor();

    // Inicializa el upscaler GPU
    bool initUpscaler(int srcW, int srcH, int dstW, int dstH, int quality = GPUUpscaler::kModeMjpegDefault);

    // Inicializa el denoiser GPU
    bool initDenoiser(int width, int height, float strength = 0.0f);

    // Procesa el frame. enableAI: Super Resolution. enableDenoise: Reduccion de ruido.
    // Si ambos estan activos, el orden es: Denoise -> Upscale.
    void processFrame(const cv::Mat& inputFrame, cv::Mat& outputFrame, bool enableAI, bool enableDenoise = false);

    // Destruye el upscaler para cambiar resoluciones
    void releaseUpscaler();

    // Destruye el denoiser
    void releaseDenoiser();

    // Verifica si el upscaler esta listo
    bool isUpscalerReady() const;

    // Verifica si el denoiser esta listo
    bool isDenoiserReady() const;

private:
    GPUUpscaler upscaler;
    GPUDenoiser denoiser;
    cv::Mat     denoisedTemp; // Buffer intermedio cuando usamos ambos efectos
};
