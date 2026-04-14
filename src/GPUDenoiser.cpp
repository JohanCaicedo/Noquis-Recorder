#include "GPUDenoiser.hpp"
#include "nvCVOpenCV.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>

static std::string getProjectDir() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    exeDir = exeDir.substr(0, lastSlash);
    size_t s1 = exeDir.find_last_of("\\/");
    std::string buildDir = exeDir.substr(0, s1);
    s1 = buildDir.find_last_of("\\/");
    return buildDir.substr(0, s1);
}

static std::string getSDKRootPath() {
    return getProjectDir() + "\\NVIDIA Video Effects";
}

static std::string getSDKBinPath() {
    return getSDKRootPath() + "\\bin";
}

static std::string getSDKFeaturePath() {
    return getSDKRootPath() + "\\features\\nvvfxdenoising\\bin";
}

static std::string getSDKModelPath() {
    return getSDKRootPath() + "\\models";
}

static std::string getSDKBinModelPath() {
    return getSDKBinPath() + "\\models";
}

static std::string getSDKFeatureModelPath() {
    return getSDKRootPath() + "\\features\\nvvfxdenoising\\models";
}

static bool hasDenoiseModels(const std::string& modelDir) {
    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::exists(modelDir, ec) || !fs::is_directory(modelDir, ec)) {
        return false;
    }

    for (const auto& entry : fs::directory_iterator(modelDir, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }

        const std::string name = entry.path().filename().string();
        if (name.rfind("Denoise-", 0) == 0 && name.find(".engine.trtpkg") != std::string::npos) {
            return true;
        }
    }

    return false;
}
#endif

// Requerido por el proxy para localizar las DLLs y modelos del SDK.
extern char* g_nvVFXSDKPath;

GPUDenoiser::GPUDenoiser()
    : effect(nullptr), stream(nullptr), stateBuffer(nullptr), stateSize(0),
      initialized(false), frameWidth(0), frameHeight(0) {
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

    auto failCuda = [&](const std::string& prefix, cudaError_t status) {
        std::cerr << prefix << cudaGetErrorString(status) << std::endl;
        release();
        return false;
    };

    std::cout << "[Denoise] Inicializando NVIDIA VFX Denoising..." << std::endl;
    std::cout << "[Denoise] Resolucion: " << width << "x" << height
              << " | Fuerza: " << strength << std::endl;

#ifdef _WIN32
    std::string sdkBin = getSDKBinPath();
    std::string featureBin = getSDKFeaturePath();

    char currentPath[32768] = {};
    GetEnvironmentVariableA("PATH", currentPath, sizeof(currentPath));
    std::string newPath = sdkBin + ";" + featureBin + ";" + currentPath;
    SetEnvironmentVariableA("PATH", newPath.c_str());

    static std::string sdkPathStorage = sdkBin;
    g_nvVFXSDKPath = &sdkPathStorage[0];

    std::cout << "[Denoise] SDK bin: " << sdkBin << std::endl;
    std::cout << "[Denoise] Feature bin: " << featureBin << std::endl;

    const std::vector<std::string> modelCandidates = {
        getSDKFeatureModelPath(),
        getSDKBinModelPath(),
        getSDKModelPath()
    };

    std::string selectedModelDir;
    for (const auto& candidate : modelCandidates) {
        const bool hasModels = hasDenoiseModels(candidate);
        std::cout << "[Denoise] Model candidate: " << candidate
                  << (hasModels ? " [OK]" : " [sin modelos detectados]") << std::endl;
        if (selectedModelDir.empty() && hasModels) {
            selectedModelDir = candidate;
        }
    }
#endif

    // Ayuda a que el SDK reporte rutas/modelos exactos cuando Load() falla.
    NvVFX_ConfigureLogger(NVCV_LOG_ERROR, "stderr", nullptr, nullptr);

    err = NvVFX_CreateEffect(NVVFX_FX_DENOISING, &effect);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error creando efecto Denoising: ", err);
    }

    err = NvVFX_CudaStreamCreate(&stream);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error creando CUDA stream: ", err);
    }

    err = NvVFX_SetCudaStream(effect, NVVFX_CUDA_STREAM, stream);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error SetCudaStream: ", err);
    }

    err = NvVFX_SetF32(effect, NVVFX_STRENGTH, strength);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error SetF32 strength: ", err);
    }

    err = NvVFX_SetU32(effect, NVVFX_MODE, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Info: Set Mode no soportado o ya configurado: "
                  << NvCV_GetErrorStringFromCode(err) << std::endl;
    }

#ifdef _WIN32
    if (!selectedModelDir.empty()) {
        err = NvVFX_SetString(effect, NVVFX_MODEL_DIRECTORY, selectedModelDir.c_str());
        if (err == NVCV_SUCCESS) {
            std::cout << "[Denoise] Model dir: " << selectedModelDir << std::endl;
        } else {
            std::cerr << "[Denoise] Info: Set ModelDir no soportado o ignorado: "
                      << NvCV_GetErrorStringFromCode(err) << std::endl;
        }
    } else {
        std::cerr << "[Denoise] Warning: no se detectaron modelos Denoise en el SDK empaquetado." << std::endl;
    }
#endif

    srcImg.create(height, width, CV_8UC3);
    dstImg.create(height, width, CV_8UC3);

    NVWrapperForCVMat(&srcImg, &srcVFX);
    NVWrapperForCVMat(&dstImg, &dstVFX);

    err = NvCVImage_Alloc(&srcGpuBuf, width, height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error asignando buffer GPU entrada: ", err);
    }

    err = NvCVImage_Alloc(&dstGpuBuf, width, height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error asignando buffer GPU salida: ", err);
    }

    err = NvCVImage_Alloc(&srcTmpBuf, width, height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error asignando buffer temporal src: ", err);
    }

    err = NvCVImage_Alloc(&dstTmpBuf, width, height, NVCV_BGR, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error asignando buffer temporal dst: ", err);
    }

    err = NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE, &srcGpuBuf);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error SetImage input: ", err);
    }

    err = NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &dstGpuBuf);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error SetImage output: ", err);
    }

    std::cout << "[Denoise] Cargando modelo de IA... (puede tardar unos segundos)" << std::endl;
    err = NvVFX_Load(effect);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[Denoise] Error cargando modelo: " << NvCV_GetErrorStringFromCode(err) << std::endl;

        cudaDeviceProp props{};
        if (cudaGetDeviceProperties(&props, 0) == cudaSuccess) {
            std::cerr << "[Denoise] GPU detectada: " << props.name
                      << " (compute capability " << props.major << "." << props.minor << ")" << std::endl;
        }

#ifdef _WIN32
        if (!selectedModelDir.empty()) {
            std::cerr << "[Denoise] Model dir usado: " << selectedModelDir << std::endl;
        }
#endif

        std::cerr << "[Denoise] Si el error persiste, reinstala los modelos del SDK para esta GPU." << std::endl;
        release();
        return false;
    }

    err = NvVFX_GetU32(effect, NVVFX_STATE_SIZE, &stateSize);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error obteniendo STATE_SIZE: ", err);
    }
    std::cout << "[Denoise] State Variable: " << stateSize << " bytes en GPU" << std::endl;

    cudaError_t cudaErr = cudaMalloc(&stateBuffer, stateSize);
    if (cudaErr != cudaSuccess) {
        return failCuda("[Denoise] Error cudaMalloc State: ", cudaErr);
    }

    cudaErr = cudaMemset(stateBuffer, 0, stateSize);
    if (cudaErr != cudaSuccess) {
        return failCuda("[Denoise] Error cudaMemset State: ", cudaErr);
    }

    err = NvVFX_SetObject(effect, NVVFX_STATE, &stateBuffer);
    if (err != NVCV_SUCCESS) {
        return failStatus("[Denoise] Error SetObject NVVFX_STATE: ", err);
    }

    initialized = true;
    std::cout << "[Denoise] Denoise ACTIVO! Fuerza: " << strength << std::endl;
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

    dstImg.copyTo(output);
    return true;
}

void GPUDenoiser::release() {
    const bool hadResources = initialized || effect || stream || stateBuffer;

    if (effect) {
        NvVFX_DestroyEffect(effect);
        effect = nullptr;
    }
    if (stream) {
        NvVFX_CudaStreamDestroy(stream);
        stream = nullptr;
    }
    if (stateBuffer) {
        cudaFree(stateBuffer);
        stateBuffer = nullptr;
    }

    stateSize = 0;
    NvCVImage_Dealloc(&srcGpuBuf);
    NvCVImage_Dealloc(&dstGpuBuf);
    NvCVImage_Dealloc(&srcTmpBuf);
    NvCVImage_Dealloc(&dstTmpBuf);
    initialized = false;

    if (hadResources) {
        std::cout << "[Denoise] Recursos GPU Denoise liberados." << std::endl;
    }
}
