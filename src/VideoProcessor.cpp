#include "VideoProcessor.hpp"
#include <iostream>

VideoProcessor::VideoProcessor() {}

VideoProcessor::~VideoProcessor() {}

bool VideoProcessor::initUpscaler(int srcW, int srcH, int dstW, int dstH, int quality) {
    return upscaler.initialize(srcW, srcH, dstW, dstH, quality);
}

void VideoProcessor::processFrame(const cv::Mat& inputFrame, cv::Mat& outputFrame, bool enableAI) {
    if (enableAI && upscaler.isReady()) {
        // Escalado real con NVIDIA VFX Super Resolution (Tensor Cores)
        if (!upscaler.upscale(inputFrame, outputFrame)) {
            // Fallback: si falla el GPU, copiar el frame original
            inputFrame.copyTo(outputFrame);
        }
    } else {
        // Pass-through: sin procesamiento
        inputFrame.copyTo(outputFrame);
    }
}

bool VideoProcessor::isUpscalerReady() const {
    return upscaler.isReady();
}

void VideoProcessor::releaseUpscaler() {
    upscaler.release();
}

