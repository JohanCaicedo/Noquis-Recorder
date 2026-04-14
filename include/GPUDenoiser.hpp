#pragma once
#include <opencv2/opencv.hpp>
#include "nvVideoEffects.h"
#include "nvCVImage.h"
#include "nvVFXDenoising.h"    // Define NVVFX_FX_DENOISING = "Denoising"
#include <cuda_runtime.h>

// Encapsula NVIDIA VFX SDK DENOISING
// Analiza multiples frames para eliminar ruido MJPEG/digital
// La resolucion de salida es IDENTICA a la de entrada (no escala)
class GPUDenoiser {
public:
    GPUDenoiser();
    ~GPUDenoiser();

    // Inicializa el efecto de Denoise
    // width/height: resolucion del video (entrada = salida)
    // strength: fuerza del filtro 0.0 (suave) a 1.0 (maximo)
    bool initialize(int width, int height, float strength = 1.0f);

    // Aplica denoise a un frame. Requiere que initialize() haya sido llamado
    bool denoise(const cv::Mat& input, cv::Mat& output);

    // Libera todos los recursos GPU (incluyendo State Variable)
    void release();

    // Verifica si el efecto esta listo para procesar
    bool isReady() const { return initialized; }

private:
    NvVFX_Handle effect;
    CUstream      stream;

    // State Variable: "memoria" del GPU para analisis multi-frame
    // El SDK acumula info de frames anteriores para distinguir ruido de detalle
    void* stateBuffer;
    unsigned int stateSize;

    NvCVImage srcGpuBuf;  // Buffer GPU de entrada (RGBA U8)
    NvCVImage dstGpuBuf;  // Buffer GPU de salida  (RGBA U8) - misma resolucion
    NvCVImage srcTmpBuf;  // Buffer temporal para transferencia CPU->GPU
    NvCVImage dstTmpBuf;  // Buffer temporal para transferencia GPU->CPU

    cv::Mat   srcImg;     // CPU buffer de entrada  (BGR U8)
    cv::Mat   dstImg;     // CPU buffer de salida   (BGR U8)
    NvCVImage srcVFX;     // Wrapper NvCVImage sobre srcImg
    NvCVImage dstVFX;     // Wrapper NvCVImage sobre dstImg

    bool initialized;
    int  frameWidth, frameHeight;
};
