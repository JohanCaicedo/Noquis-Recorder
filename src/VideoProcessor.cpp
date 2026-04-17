#include "VideoProcessor.hpp"
#include <iostream>

VideoProcessor::VideoProcessor() {}

VideoProcessor::~VideoProcessor() {}

bool VideoProcessor::initUpscaler(int srcW, int srcH, int dstW, int dstH, int quality) {
    return upscaler.initialize(srcW, srcH, dstW, dstH, quality);
}

bool VideoProcessor::initDenoiser(int width, int height, float strength) {
    return denoiser.initialize(width, height, strength);
}

void VideoProcessor::processFrame(const cv::Mat& inputFrame, cv::Mat& outputFrame, bool enableAI, bool enableDenoise) {
    // MODO: Denoise + Super Resolution (cadena directa VRAM-to-VRAM)
    if (enableDenoise && enableAI && denoiser.isReady() && upscaler.isReady()) {
        if (denoiser.denoiseToGPU(inputFrame)) {
            // Pasamos el puntero de la memoria gráfica directamente
            if (!upscaler.upscaleFromGPU(denoiser.getOutputGPUBuffer(), outputFrame)) {
                inputFrame.copyTo(outputFrame);
            }
        } else {
            inputFrame.copyTo(outputFrame);
        }
        return;
    }

    // MODO: Solo Super Resolution
    if (enableAI && upscaler.isReady()) {
        if (!upscaler.upscale(inputFrame, outputFrame)) {
            inputFrame.copyTo(outputFrame);
        }
        return;
    }

    // MODO: Solo Denoise (limpia ruido, misma resolucion)
    if (enableDenoise && denoiser.isReady()) {
        if (!denoiser.denoise(inputFrame, outputFrame)) {
            inputFrame.copyTo(outputFrame);
        }
        return;
    }

    // MODO: Pass-through (sin procesamiento)
    inputFrame.copyTo(outputFrame);
}

bool VideoProcessor::isUpscalerReady() const {
    return upscaler.isReady();
}

bool VideoProcessor::isDenoiserReady() const {
    return denoiser.isReady();
}

void VideoProcessor::releaseUpscaler() {
    upscaler.release();
}

void VideoProcessor::releaseDenoiser() {
    denoiser.release();
}
