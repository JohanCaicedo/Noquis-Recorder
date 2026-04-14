#include "GPUDenoiser.hpp"
#include "nvCVOpenCV.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// Obtiene la ruta del sdk bin/ del sistema
static std::string getDenoiserSDKBinPath() {
    return "C:\\Program Files\\NVIDIA Corporation\\NVIDIA Video Effects\\bin";
}

GPUDenoiser::GPUDenoiser()
    : effect(nullptr), stream(nullptr), stateBuffer(nullptr), stateSize(0),
      initialized(false), frameWidth(0), frameHeight(0) {
    memset(&srcGpuBuf, 0, sizeof(NvCVImage));
    memset(&dstGpuBuf, 0, sizeof(NvCVImage));
    memset(&srcTmpBuf, 0, sizeof(NvCVImage));
    memset(&dstTmpBuf, 0, sizeof(NvCVImage));
    memset(&srcVFX,    0, sizeof(NvCVImage));
    memset(&dstVFX,    0, sizeof(NvCVImage));
}

GPUDenoiser::~GPUDenoiser() {
    release();
}

bool GPUDenoiser::initialize(int width, int height, float strength) {
    NvCV_Status err;
    frameWidth  = width;
    frameHeight = height;

    std::cout << "[Denoise] Inicializando NVIDIA VFX Denoising..." << std::endl;
    std::cout << "[Denoise] Resolucion: " << width << "x" << height
              << " | Fuerza: " << strength << std::endl;

    // ============================================================
    // Configurar rutas del SDK para carga de DLLs
    // ============================================================
#ifdef _WIN32
    std::string sdkBin     = getDenoiserSDKBinPath();
    std::string featureBin = "C:\\Program Files\\NVIDIA Corporation\\NVIDIA Video Effects\\features\\nvvfxdenoising\\bin";

    // Agregar al PATH para que Windows encuentre las DLLs
    char currentPath[32768];
    GetEnvironmentVariableA("PATH", currentPath, sizeof(currentPath));
    std::string newPath = sdkBin + ";" + featureBin + ";" + currentPath;
    SetEnvironmentVariableA("PATH", newPath.c_str());
    SetDllDirectoryA(sdkBin.c_str());

    std::cout << "[Denoise] SDK bin: " << sdkBin << std::endl;
    std::cout << "[Denoise] Feature bin: " << featureBin << std::endl;
#endif

    // 1. Crear el efecto Denoising
    err = NvVFX_CreateEffect(NVVFX_FX_DENOISING, &effect);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error creando efecto: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 2. Crear CUDA stream
    err = NvVFX_CudaStreamCreate(&stream);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error creando CUDA stream: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 3. Configurar la fuerza del filtro (0.0 = suave, 1.0 = máximo)
    err = NvVFX_SetF32(effect, NVVFX_STRENGTH, strength);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error SetF32 strength: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 4. Preparar buffers CPU (cv::Mat) - Entrada y salida son el mismo tamaño
    srcImg.create(height, width, CV_8UC3);
    dstImg.create(height, width, CV_8UC3);

    // 5. Crear wrappers NvCVImage sobre los cv::Mat
    NVWrapperForCVMat(&srcImg, &srcVFX);
    NVWrapperForCVMat(&dstImg, &dstVFX);

    // 6. Asignar buffers GPU RGBA (requiere el SDK)
    err = NvCVImage_Alloc(&srcGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error asignando buffer GPU entrada: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    err = NvCVImage_Alloc(&dstGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error asignando buffer GPU salida: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 7. Buffers temporales para las transferencias CPU<->GPU
    err = NvCVImage_Alloc(&srcTmpBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error asignando buffer temporal src: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    err = NvCVImage_Alloc(&dstTmpBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error asignando buffer temporal dst: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 8. Vincular buffers al efecto
    err = NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE, &srcGpuBuf);
    if (err != NVCV_SUCCESS) { std::cerr << "[Denoise] Error SetImage input: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    err = NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &dstGpuBuf);
    if (err != NVCV_SUCCESS) { std::cerr << "[Denoise] Error SetImage output: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    err = NvVFX_SetCudaStream(effect, NVVFX_CUDA_STREAM, stream);
    if (err != NVCV_SUCCESS) { std::cerr << "[Denoise] Error SetCudaStream: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    // 9. ==============================================================
    // STATE VARIABLE (CRITICO para denoise temporal)
    // El SDK necesita "memoria" en la GPU donde acumula info de frames
    // anteriores. Sin esto, el denoise no funciona correctamente.
    // ==============================================================

    // Preguntar al SDK cuantos bytes necesita
    err = NvVFX_GetU32(effect, NVVFX_STATE_SIZE, &stateSize);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error obteniendo STATE_SIZE: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }
    std::cout << "[Denoise] State Variable requerida: " << stateSize << " bytes en GPU" << std::endl;

    // Allocar en la GPU y limpiar a cero (estado inicial)
    cudaError_t cudaErr = cudaMalloc(&stateBuffer, stateSize);
    if (cudaErr != cudaSuccess) {
        std::cerr << "[Denoise] Error cudaMalloc State: " << cudaGetErrorString(cudaErr) << std::endl;
        return false;
    }
    cudaMemset(stateBuffer, 0, stateSize);

    // Vincular la state variable al efecto
    err = NvVFX_SetObject(effect, NVVFX_STATE, &stateBuffer);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error SetObject NVVFX_STATE: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 10. Cargar el modelo de IA
    std::cout << "[Denoise] Cargando modelo de IA... (puede tardar unos segundos)" << std::endl;
    err = NvVFX_Load(effect);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error cargando modelo: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    initialized = true;
    std::cout << "[Denoise] Denoise ACTIVO! Fuerza: " << strength << std::endl;
    return true;
}

bool GPUDenoiser::denoise(const cv::Mat& input, cv::Mat& output) {
    if (!initialized || !effect) return false;

    NvCV_Status err;

    // Copiar el frame de entrada al buffer interno
    if (input.cols != srcImg.cols || input.rows != srcImg.rows) {
        cv::resize(input, srcImg, srcImg.size());
    } else {
        input.copyTo(srcImg);
    }

    // Actualizar el wrapper sobre el cv::Mat
    NVWrapperForCVMat(&srcImg, &srcVFX);

    // Transfer CPU -> GPU (BGR U8 -> RGBA U8 con conversion automatica)
    err = NvCVImage_Transfer(&srcVFX, &srcGpuBuf, 1.0f, stream, &srcTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error transfer src->GPU: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // Ejecutar Denoise en GPU (usa la State Variable para analizar multiples frames)
    err = NvVFX_Run(effect, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error NvVFX_Run: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // Transfer GPU -> CPU (RGBA U8 -> BGR U8)
    NVWrapperForCVMat(&dstImg, &dstVFX);
    err = NvCVImage_Transfer(&dstGpuBuf, &dstVFX, 1.0f, stream, &dstTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error transfer GPU->dst: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    dstImg.copyTo(output);
    return true;
}

void GPUDenoiser::release() {
    if (effect) {
        NvVFX_DestroyEffect(effect);
        effect = nullptr;
    }
    if (stream) {
        NvVFX_CudaStreamDestroy(stream);
        stream = nullptr;
    }
    // Liberar la State Variable del GPU (importante: esto es cudaFree, no delete)
    if (stateBuffer) {
        cudaFree(stateBuffer);
        stateBuffer = nullptr;
        stateSize   = 0;
    }
    NvCVImage_Dealloc(&srcGpuBuf);
    NvCVImage_Dealloc(&dstGpuBuf);
    NvCVImage_Dealloc(&srcTmpBuf);
    NvCVImage_Dealloc(&dstTmpBuf);
    initialized = false;
    std::cout << "[Denoise] Recursos GPU Denoise liberados." << std::endl;
}
