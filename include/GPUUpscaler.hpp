#pragma once
#include <opencv2/opencv.hpp>
#include "nvVideoEffects.h"
#include "nvCVImage.h"
#include "nvVFXVideoSuperRes.h"

// Clase que encapsula NVIDIA VFX SDK para Super Resolucion en GPU
// Escala 720p -> 1440p usando los Tensor Cores de la RTX 5070
class GPUUpscaler {
public:
    GPUUpscaler();
    ~GPUUpscaler();

    // Inicializa el efecto de Super Resolucion
    // srcWidth/srcHeight: resolucion de entrada (ej: 1280x720)
    // dstWidth/dstHeight: resolucion de salida (ej: 2560x1440)
    // qualityMode: 0=Bicubic, 1=Low, 2=Medium, 3=High, 4=Ultra
    bool initialize(int srcWidth, int srcHeight, int dstWidth, int dstHeight, int qualityMode = 1);

    // Escala un frame de entrada y devuelve el frame escalado
    bool upscale(const cv::Mat& input, cv::Mat& output);

    // Libera todos los recursos GPU
    void release();

    // Verifica si el efecto esta listo
    bool isReady() const { return initialized; }

private:
    NvVFX_Handle effect;
    CUstream stream;

    NvCVImage srcGpuBuf;  // Buffer GPU de entrada (RGBA U8)
    NvCVImage dstGpuBuf;  // Buffer GPU de salida  (RGBA U8)
    NvCVImage srcTmpBuf;  // Buffer temporal para transferencias
    NvCVImage dstTmpBuf;  // Buffer temporal para transferencias

    cv::Mat srcImg;       // CPU buffer de entrada  (BGR U8)
    cv::Mat dstImg;       // CPU buffer de salida   (BGR U8)
    NvCVImage srcVFX;     // Wrapper NvCVImage sobre srcImg
    NvCVImage dstVFX;     // Wrapper NvCVImage sobre dstImg

    bool initialized;
    int outWidth, outHeight;
};
