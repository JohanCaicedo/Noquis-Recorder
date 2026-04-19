#include "GPUDenoiser.hpp"
#include "nvCVOpenCV.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// Requerido por el proxy para localizar las DLLs del SDK
extern char* g_nvVFXSDKPath;

static std::string getExecutableDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return exePath.substr(0, lastSlash);
    }
#endif
    return "";
}

static std::string getSDKBinPath() {
    return getExecutableDir();
}

static std::string getSDKFeaturePath() {
    return getExecutableDir();
}

GPUDenoiser::GPUDenoiser()
    : effect(nullptr), stream(nullptr), initialized(false), frameWidth(0), frameHeight(0) {
    memset(&srcGpuBuf, 0, sizeof(NvCVImage));
    memset(&dstGpuBuf, 0, sizeof(NvCVImage));
    memset(&srcTmpBuf, 0, sizeof(NvCVImage));
    memset(&dstTmpBuf, 0, sizeof(NvCVImage));
    memset(&srcVFX, 0, sizeof(NvCVImage));
    memset(&dstVFX, 0, sizeof(NvCVImage));
}

GPUDenoiser::~GPUDenoiser() {
    release();
}

bool GPUDenoiser::initialize(int width, int height, float strength) {
    release();

    NvCV_Status err;
    frameWidth = width;
    frameHeight = height;

    auto failStatus = [&](const std::string& prefix, NvCV_Status status) {
        std::cerr << prefix << NvCV_GetErrorStringFromCode(status) << std::endl;
        release();
        return false;
    };

    std::cout << "[Denoise] Inicializando RTX Artifact Reduction..." << std::endl;
    
    // Mapeo logico para el nivel de calidad de Reduccion de Artefactos de VideoSuperRes
    // 8=Low, 9=Medium, 10=High, 11=Ultra
    int qualityLevel = 8;
    if (strength > 0.0f && strength <= 0.33f) qualityLevel = 9;
    else if (strength > 0.33f && strength <= 0.66f) qualityLevel = 10;
    else if (strength > 0.66f) qualityLevel = 11;

    std::cout << "[Denoise] Resolucion: " << width << "x" << height
              << " | Modo VSR_Denoise: " << qualityLevel << std::endl;

#ifdef _WIN32
    std::string sdkBin = getSDKBinPath();
    std::string featureBin = getSDKFeaturePath();

    char currentPath[32768] = {};
    GetEnvironmentVariableA("PATH", currentPath, sizeof(currentPath));
    std::string newPath = sdkBin + ";" + featureBin + ";" + currentPath;
    SetEnvironmentVariableA("PATH", newPath.c_str());

    static std::string sdkPathStorage = sdkBin;
    g_nvVFXSDKPath = &sdkPathStorage[0];
#endif

    // Este efecto carga RTX VSR (el cual contiene un submodelo de limpieza espacial superior)
    err = NvVFX_CreateEffect(NVVFX_FX_VIDEO_SUPER_RES, &effect);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error creando efecto VideoSuperRes (Artifact Mode): ", err);
    }

    err = NvVFX_CudaStreamCreate(&stream);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error creando CUDA stream: ", err);
    }

    // Configurar resoluciones de buffers de CPU
    srcImg.create(height, width, CV_8UC3);
    dstImg.create(height, width, CV_8UC3);
    NVWrapperForCVMat(&srcImg, &srcVFX);
    NVWrapperForCVMat(&dstImg, &dstVFX);

    // Buffers GPU deben ser RGBA para NVVFX_FX_VIDEO_SUPER_RES
    err = NvCVImage_Alloc(&srcGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error asig buffer GPU entrada: ", err);

    err = NvCVImage_Alloc(&dstGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error asig buffer GPU salida: ", err);

    err = NvCVImage_Alloc(&srcTmpBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error asig buffer temporal src: ", err);

    err = NvCVImage_Alloc(&dstTmpBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error asig buffer temporal dst: ", err);

    // Conectar inputs y parameters
    err = NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE, &srcGpuBuf);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error SetImage input: ", err);

    err = NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &dstGpuBuf);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error SetImage output: ", err);

    err = NvVFX_SetCudaStream(effect, NVVFX_CUDA_STREAM, stream);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error SetCudaStream: ", err);

    err = NvVFX_SetU32(effect, NVVFX_QUALITY_LEVEL, qualityLevel);
    if (err != NVCV_SUCCESS) return failStatus("[Denoise] Error SetU32 Quality: ", err);

    // Habilitamos los logs internos verdaderos de NVIDIA para ver telemetria
    NvVFX_ConfigureLogger(NVCV_LOG_INFO, "nvvfx_denoise.log", nullptr, nullptr);

    std::cout << "[Denoise] Cargando modelo RTX VSR Artifact Reduction... " << std::endl;
    err = NvVFX_Load(effect);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error cargando modelo: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        release();
        return false;
    }

    initialized = true;
    std::cout << "[Denoise] Denoise / Artifact Reduction ACTIVO! Modo: " << qualityLevel << std::endl;
    return true;
}

bool GPUDenoiser::denoise(const cv::Mat& input, cv::Mat& output) {
    if (!initialized || !effect) return false;

    NvCV_Status err;

    if (input.cols != srcImg.cols || input.rows != srcImg.rows) {
        cv::resize(input, srcImg, srcImg.size());
    } else {
        input.copyTo(srcImg);
    }

    NVWrapperForCVMat(&srcImg, &srcVFX);

    err = NvCVImage_Transfer(&srcVFX, &srcGpuBuf, 1.0f, stream, &srcTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error transfer src->GPU: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    err = NvVFX_Run(effect, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error NvVFX_Run: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    NVWrapperForCVMat(&dstImg, &dstVFX);
    err = NvCVImage_Transfer(&dstGpuBuf, &dstVFX, 1.0f, stream, &dstTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error transfer GPU->dst: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    cudaStreamSynchronize(stream);
    dstImg.copyTo(output);
    return true;
}

bool GPUDenoiser::denoiseToGPU(const cv::Mat& input) {
    if (!initialized || !effect) return false;

    NvCV_Status err;

    if (input.cols != srcImg.cols || input.rows != srcImg.rows) {
        cv::resize(input, srcImg, srcImg.size());
    } else {
        input.copyTo(srcImg);
    }

    NVWrapperForCVMat(&srcImg, &srcVFX);

    err = NvCVImage_Transfer(&srcVFX, &srcGpuBuf, 1.0f, stream, &srcTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error transfer src->GPU: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    err = NvVFX_Run(effect, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error NvVFX_Run: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // Detenemos el flujo aquí: el filtro ha corrido y el frame validado final 
    // yace comodamente en `dstGpuBuf`. Sincronizamos para asegurar su integridad.
    cudaStreamSynchronize(stream);
    return true;
}

void GPUDenoiser::release() {
    const bool hadResources = initialized || effect || stream;

    if (effect) {
        NvVFX_DestroyEffect(effect);
        effect = nullptr;
    }
    if (stream) {
        NvVFX_CudaStreamDestroy(stream);
        stream = nullptr;
    }

    NvCVImage_Dealloc(&srcGpuBuf);
    NvCVImage_Dealloc(&dstGpuBuf);
    NvCVImage_Dealloc(&srcTmpBuf);
    NvCVImage_Dealloc(&dstTmpBuf);
    initialized = false;

    if (hadResources) {
        std::cout << "[Denoise] Recursos GPU Denoise liberados." << std::endl;
    }
}
