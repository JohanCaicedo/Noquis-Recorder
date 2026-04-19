cmake_minimum_required(VERSION 3.15)

project(AILinkCapture)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Remover flags manuales para evitar choque con Debug
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Buscar OpenCV (vcpkg lo proveerá)
find_package(OpenCV REQUIRED)

# CUDA Toolkit (necesario para cudaMalloc/cudaFree del State Variable del Denoiser)
find_package(CUDAToolkit REQUIRED)

# ============================================================
# NVIDIA VFX SDK (Video Effects / Super Resolution)
# ============================================================
set(VFXSDK_ROOT "${CMAKE_SOURCE_DIR}/NVIDIA Video Effects")
set(OPTICAL_FLOW_SDK_ROOT "${CMAKE_SOURCE_DIR}/third_party/NVIDIA Optical Flow SDK")
set(ANIME4KCPP_ROOT "${CMAKE_SOURCE_DIR}/third_party/Anime4KCPP")

# ============================================================
# Anime4KCPP (core only, sin modulo de video)
# ============================================================
set(AC_BUILD_CLI OFF CACHE BOOL "" FORCE)
set(AC_BUILD_GUI OFF CACHE BOOL "" FORCE)
set(AC_BUILD_VIDEO OFF CACHE BOOL "" FORCE)
set(AC_BUILD_FILTER_AVISYNTH OFF CACHE BOOL "" FORCE)
set(AC_BUILD_FILTER_VAPOURSYNTH OFF CACHE BOOL "" FORCE)
set(AC_BUILD_FILTER_DIRECTSHOW OFF CACHE BOOL "" FORCE)
set(AC_BUILD_FILTER_AVISYNTH_VAPOURSYNTH OFF CACHE BOOL "" FORCE)
set(AC_BUILD_BINDING_C OFF CACHE BOOL "" FORCE)
set(AC_BUILD_BINDING_PYTHON OFF CACHE BOOL "" FORCE)
set(AC_TOOLS_BENCHMARK OFF CACHE BOOL "" FORCE)
set(AC_TEST_UTIL OFF CACHE BOOL "" FORCE)
set(AC_TEST_VIDEO OFF CACHE BOOL "" FORCE)
set(AC_CORE_DISABLE_IMAGE_IO ON CACHE BOOL "" FORCE)
set(AC_CORE_WITH_CUDA OFF CACHE BOOL "" FORCE)
set(AC_CORE_WITH_OPENCL OFF CACHE BOOL "" FORCE)
add_subdirectory("${ANIME4KCPP_ROOT}" "${CMAKE_BINARY_DIR}/third_party/anime4kcpp")

# Carpetas de cabeceras
include_directories(include)
include_directories("${VFXSDK_ROOT}/nvvfx/include")
# Feature headers (VideoSuperRes)
include_directories("${VFXSDK_ROOT}/features/nvvfxvideosuperres/include")
# Feature headers (Denoising)
include_directories("${VFXSDK_ROOT}/features/nvvfxdenoising/include")
# Utilidades del SDK (nvCVOpenCV.h)
include_directories("${CMAKE_SOURCE_DIR}/../VFX-SDK-Samples/utils")
include_directories("${OPTICAL_FLOW_SDK_ROOT}/NvOFFRUC/Interface")
include_directories("${OPTICAL_FLOW_SDK_ROOT}/NvOFInterface")

# Archivos fuente de la aplicacion
set(SOURCES
    main.cpp
    src/CaptureManager.cpp
    src/VideoProcessor.cpp
    src/AppWindow.cpp
    src/Application.cpp
    src/Anime4KUpscaler.cpp
    src/GPUFrameGenerator.cpp
    src/GPUUpscaler.cpp
    src/GPUDenoiser.cpp
    src/OpenCVUpscaler.cpp
    src/AudioManager.cpp
    src/ConfigManager.cpp
    resource.rc
    # Proxy files del VFX SDK (carga dinamica de DLLs)
    "${VFXSDK_ROOT}/nvvfx/src/nvVideoEffectsProxy.cpp"
    "${VFXSDK_ROOT}/nvvfx/src/nvCVImageProxy.cpp"
)

# Ejecutable
add_executable(AILinkCapture ${SOURCES})
set_target_properties(AILinkCapture PROPERTIES OUTPUT_NAME "Gnocchi's Viewer")
target_compile_definitions(AILinkCapture PRIVATE CAPTURADORA_PROJECT_DIR="${CMAKE_SOURCE_DIR}")
target_compile_definitions(AILinkCapture PRIVATE CAPTURADORA_OPTICAL_FLOW_SDK_DIR="${OPTICAL_FLOW_SDK_ROOT}")

# Conectar OpenCV
target_link_libraries(AILinkCapture PRIVATE ${OpenCV_LIBS})
target_link_libraries(AILinkCapture PRIVATE AC::Core)

# Conectar CUDA Runtime (para cudaMalloc/cudaFree en GPUDenoiser)
target_link_libraries(AILinkCapture PRIVATE CUDA::cudart)
target_link_libraries(AILinkCapture PRIVATE Mfplat Mf Mfuuid Mfreadwrite Ole32)

# Librerias nativas de Windows
if(MSVC)
    # Aplicar optimizacion /O2 solo en Release para no chocar con /RTC1 en Debug
    target_compile_options(AILinkCapture PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

# ============================================================
# AUTONOMIA DEL EJECUTABLE POST-BUILD (PORTABILIDAD)
# ============================================================
# Le decimos a CMake que arrastre automáticamente todo el ecosistema de 
# NVIDIA (Modelos de IA .TRT y los nucleos .DLL) y los empaquete justo 
# al lado del ejecutable final cada vez que compiles.
add_custom_command(TARGET AILinkCapture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VFXSDK_ROOT}/bin"
        "$<TARGET_FILE_DIR:AILinkCapture>"
    COMMENT "Haciendo la aplicacion portable: Empaquetando DLLs y Modelos de NVIDIA IA..."
)

add_custom_command(TARGET AILinkCapture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VFXSDK_ROOT}/features/nvvfxvideosuperres/bin"
        "$<TARGET_FILE_DIR:AILinkCapture>"
    COMMENT "Empaquetando NVIDIA VideoSuperRes Plugin..."
)

add_custom_command(TARGET AILinkCapture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${VFXSDK_ROOT}/features/nvvfxdenoising/bin"
        "$<TARGET_FILE_DIR:AILinkCapture>"
    COMMENT "Empaquetando NVIDIA Denoising Plugin..."
)

# ============================================================
# EMPAQUETADO DE DEPENDENCIAS VCPKG (OPENCV DLLS)
# ============================================================
add_custom_command(TARGET AILinkCapture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/vcpkg_installed/x64-windows/bin"
        "$<TARGET_FILE_DIR:AILinkCapture>"
    COMMENT "Empaquetando dependencias de OpenCV (DLLs)..."
)

if(EXISTS "${OPTICAL_FLOW_SDK_ROOT}/NvOFFRUC/NvOFFRUCSample/bin/win64/NvOFFRUC.dll")
    add_custom_command(TARGET AILinkCapture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPTICAL_FLOW_SDK_ROOT}/NvOFFRUC/NvOFFRUCSample/bin/win64/NvOFFRUC.dll"
            "$<TARGET_FILE_DIR:AILinkCapture>/NvOFFRUC.dll"
        COMMENT "Empaquetando NVIDIA Optical Flow FRUC runtime..."
    )
endif()

if(EXISTS "${OPTICAL_FLOW_SDK_ROOT}/NvOFFRUC/NvOFFRUCSample/bin/win64/cudart64_110.dll")
    add_custom_command(TARGET AILinkCapture POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${OPTICAL_FLOW_SDK_ROOT}/NvOFFRUC/NvOFFRUCSample/bin/win64/cudart64_110.dll"
            "$<TARGET_FILE_DIR:AILinkCapture>/cudart64_110.dll"
        COMMENT "Empaquetando runtime CUDA requerido por FRUC..."
    )
endif()

# ============================================================
# EMPAQUETADO DE CONFIGURACION Y DOCUMENTACION
# ============================================================
add_custom_command(TARGET AILinkCapture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/config.ini"
        "$<TARGET_FILE_DIR:AILinkCapture>/config.ini"
    COMMENT "Empaquetando archivo de configuracion (config.ini)..."
)

add_custom_command(TARGET AILinkCapture POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/README.md"
        "$<TARGET_FILE_DIR:AILinkCapture>/README.md"
    COMMENT "Empaquetando documentacion (README.md)..."
)
