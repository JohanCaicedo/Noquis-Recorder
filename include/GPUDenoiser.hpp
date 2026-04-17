#pragma once
#include <opencv2/opencv.hpp>
#include "nvVideoEffects.h"
#include "nvCVImage.h"
#include "nvVFXVideoSuperRes.h"
#include <cuda_runtime.h>

// Encapsula NVIDIA VFX SDK ARTIFACT REDUCTION (Modos de Denoise de VideoSuperRes)
// Limpia macrobloques y ruido de compresion MJPEG sin usar historial temporal,
// ideal para alta velocidad y juegos (evita ghosting).
// La resolucion de salida es IDENTICA a la de entrada (no escala)
class GPUDenoiser {
public:
    GPUDenoiser();
    ~GPUDenoiser();

    // Inicializa el efecto de Denoise (Artifact Reduction)
    // width/height: resolucion del video (entrada = salida)
    // strength: 0.0 preserva mas textura, 1.0 elimina mas ruido MJPEG
    bool initialize(int width, int height, float strength = 0.0f);

    // Aplica denoise a un frame y transfiere de memoria GPU devuelta a CPU
    bool denoise(const cv::Mat& input, cv::Mat& output);

    // Aplica denoise y detiene el flujo en CUDA, manteniendo el frame en VRAM pura
    bool denoiseToGPU(const cv::Mat& input);

    // Devuelve el puntero hacia la memoria grafica VRAM del frame limpio
    const NvCVImage* getOutputGPUBuffer() const { return &dstGpuBuf; }

    // Libera todos los recursos GPU
    void release();

    // Verifica si el efecto esta listo para procesar
    bool isReady() const { return initialized; }

private:
    NvVFX_Handle effect;
    CUstream      stream;

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
