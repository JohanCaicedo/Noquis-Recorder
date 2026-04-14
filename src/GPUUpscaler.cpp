#include "GPUUpscaler.hpp"
#include "nvCVOpenCV.h"
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

// Requerido por el proxy para localizar las DLLs del SDK
char* g_nvVFXSDKPath = NULL;

// Helper: construir la ruta absoluta del SDK relativo al .exe
static std::string getSDKBinPath() {
#ifdef _WIN32
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    std::string exeDir(exePath);
    size_t lastSlash = exeDir.find_last_of("\\/");
    exeDir = exeDir.substr(0, lastSlash);
    // Subimos 2 niveles: build/Release -> raiz del proyecto
    size_t s1 = exeDir.find_last_of("\\/");
    std::string buildDir = exeDir.substr(0, s1);
    s1 = buildDir.find_last_of("\\/");
    std::string projectDir = buildDir.substr(0, s1);
    return projectDir + "\\VideoFX\\bin";
#else
    return "";
#endif
}

GPUUpscaler::GPUUpscaler() 
    : effect(nullptr), stream(nullptr), initialized(false), outWidth(0), outHeight(0) {
    memset(&srcGpuBuf, 0, sizeof(NvCVImage));
    memset(&dstGpuBuf, 0, sizeof(NvCVImage));
    memset(&srcTmpBuf, 0, sizeof(NvCVImage));
    memset(&dstTmpBuf, 0, sizeof(NvCVImage));
    memset(&srcVFX, 0, sizeof(NvCVImage));
    memset(&dstVFX, 0, sizeof(NvCVImage));
}

GPUUpscaler::~GPUUpscaler() {
    release();
}

bool GPUUpscaler::initialize(int srcWidth, int srcHeight, int dstWidth, int dstHeight, int qualityMode) {
    NvCV_Status err;

    outWidth = dstWidth;
    outHeight = dstHeight;

    std::cout << "[GPU] Inicializando NVIDIA VFX Super Resolution..." << std::endl;
    std::cout << "[GPU] Escalado: " << srcWidth << "x" << srcHeight 
              << " -> " << dstWidth << "x" << dstHeight << std::endl;

    // ============================================
    // Configurar rutas del SDK ANTES de crear el efecto
    // ============================================
#ifdef _WIN32
    std::string sdkBin = getSDKBinPath();
    std::cout << "[GPU] SDK Path: " << sdkBin << std::endl;
    
    // Apuntar g_nvVFXSDKPath al bin/ del SDK (proxy usa esto)
    static std::string sdkPathStorage = sdkBin;
    g_nvVFXSDKPath = &sdkPathStorage[0];

    // Agregar el feature VideoSuperRes al PATH para que Windows lo encuentre
    std::string sdkRoot = sdkBin.substr(0, sdkBin.find_last_of("\\/"));
    std::string featureBin = sdkRoot + "\\features\\nvvfxvideosuperres\\bin";
    
    // Construir nuevo PATH con las rutas del SDK
    char currentPath[32768];
    GetEnvironmentVariableA("PATH", currentPath, sizeof(currentPath));
    std::string newPath = sdkBin + ";" + featureBin + ";" + currentPath;
    SetEnvironmentVariableA("PATH", newPath.c_str());
    
    // También SetDllDirectory para el directorio principal del SDK
    SetDllDirectoryA(sdkBin.c_str());
    
    std::cout << "[GPU] Feature path: " << featureBin << std::endl;
#endif

    // 1. Crear el efecto VideoSuperRes
    err = NvVFX_CreateEffect(NVVFX_FX_VIDEO_SUPER_RES, &effect);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error creando efecto VideoSuperRes: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 2. Crear CUDA stream
    err = NvVFX_CudaStreamCreate(&stream);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error creando CUDA stream: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 3. Preparar buffers CPU (cv::Mat)
    srcImg.create(srcHeight, srcWidth, CV_8UC3);
    dstImg.create(dstHeight, dstWidth, CV_8UC3);

    // 4. Crear wrappers NvCVImage sobre los cv::Mat
    NVWrapperForCVMat(&srcImg, &srcVFX);
    NVWrapperForCVMat(&dstImg, &dstVFX);

    // 5. Asignar buffers GPU (RGBA U8 como requiere VideoSuperRes)
    err = NvCVImage_Alloc(&srcGpuBuf, srcWidth, srcHeight, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error asignando buffer GPU de entrada: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    err = NvCVImage_Alloc(&dstGpuBuf, dstWidth, dstHeight, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 32);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error asignando buffer GPU de salida: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 6. Asignar buffers temporales para transferencias CPU<->GPU
    err = NvCVImage_Alloc(&srcTmpBuf, srcWidth, srcHeight, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error asignando buffer temporal src: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    err = NvCVImage_Alloc(&dstTmpBuf, dstWidth, dstHeight, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_GPU, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error asignando buffer temporal dst: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // 7. Configurar el efecto
    err = NvVFX_SetImage(effect, NVVFX_INPUT_IMAGE, &srcGpuBuf);
    if (err != NVCV_SUCCESS) { std::cerr << "[GPU] Error SetImage input: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    err = NvVFX_SetImage(effect, NVVFX_OUTPUT_IMAGE, &dstGpuBuf);
    if (err != NVCV_SUCCESS) { std::cerr << "[GPU] Error SetImage output: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    err = NvVFX_SetCudaStream(effect, NVVFX_CUDA_STREAM, stream);
    if (err != NVCV_SUCCESS) { std::cerr << "[GPU] Error SetCudaStream: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    // Modo de calidad: 0=Bicubic, 1=Low, 2=Medium, 3=High, 4=Ultra
    err = NvVFX_SetU32(effect, NVVFX_QUALITY_LEVEL, (unsigned int)qualityMode);
    if (err != NVCV_SUCCESS) { std::cerr << "[GPU] Error SetU32 quality: " << NvCV_GetErrorStringFromCode(err) << std::endl; return false; }

    // 8. Cargar el modelo (esto tarda unos segundos la primera vez)
    std::cout << "[GPU] Cargando modelo de IA... (esto puede tardar unos segundos)" << std::endl;
    err = NvVFX_Load(effect);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error cargando modelo: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    initialized = true;
    std::cout << "[GPU] Super Resolution ACTIVA! Modo calidad: " << qualityMode << std::endl;
    return true;
}

bool GPUUpscaler::upscale(const cv::Mat& input, cv::Mat& output) {
    if (!initialized || !effect) return false;

    NvCV_Status err;

    // Copiar el frame de entrada al cv::Mat interno (srcImg)
    // Asegurar que el tamaño coincide
    if (input.cols != srcImg.cols || input.rows != srcImg.rows) {
        cv::resize(input, srcImg, srcImg.size());
    } else {
        input.copyTo(srcImg);
    }

    // Actualizar el wrapper
    NVWrapperForCVMat(&srcImg, &srcVFX);

    // Transferir CPU -> GPU (BGR U8 -> RGBA U8 con conversion automatica)
    err = NvCVImage_Transfer(&srcVFX, &srcGpuBuf, 1.0f, stream, &srcTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error transfer src->GPU: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // Ejecutar Super Resolution en GPU
    err = NvVFX_Run(effect, 0);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error NvVFX_Run: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // Transferir GPU -> CPU (RGBA U8 -> BGR U8)
    NVWrapperForCVMat(&dstImg, &dstVFX);
    err = NvCVImage_Transfer(&dstGpuBuf, &dstVFX, 1.0f, stream, &dstTmpBuf);
    if (err != NVCV_SUCCESS) {
        std::cerr << "[GPU] Error transfer GPU->dst: " << NvCV_GetErrorStringFromCode(err) << std::endl;
        return false;
    }

    // Copiar resultado al output
    dstImg.copyTo(output);
    return true;
}

void GPUUpscaler::release() {
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
    std::cout << "[GPU] Recursos GPU liberados." << std::endl;
}
