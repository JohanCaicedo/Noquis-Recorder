cmake_minimum_required(VERSION 3.15)

project(AILinkCapture)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Remover flags manuales para evitar choque con Debug
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Buscar OpenCV (vcpkg lo proveerá)
find_package(OpenCV REQUIRED)

# ============================================================
# NVIDIA VFX SDK (Video Effects / Super Resolution)
# ============================================================
set(VFXSDK_ROOT "${CMAKE_SOURCE_DIR}/VideoFX")

# Carpetas de cabeceras
include_directories(include)
include_directories("${VFXSDK_ROOT}/nvvfx/include")
# Feature headers (VideoSuperRes)
include_directories("${VFXSDK_ROOT}/features/nvvfxvideosuperres/include")
# Utilidades del SDK (nvCVOpenCV.h)
include_directories("${CMAKE_SOURCE_DIR}/../VFX-SDK-Samples/utils")

# Archivos fuente de la aplicacion
set(SOURCES
    main.cpp
    src/CaptureManager.cpp
    src/VideoProcessor.cpp
    src/AppWindow.cpp
    src/Application.cpp
    src/GPUUpscaler.cpp
    src/AudioManager.cpp
    # Proxy files del VFX SDK (carga dinamica de DLLs)
    "${VFXSDK_ROOT}/nvvfx/src/nvVideoEffectsProxy.cpp"
    "${VFXSDK_ROOT}/nvvfx/src/nvCVImageProxy.cpp"
)

# Ejecutable
add_executable(AILinkCapture ${SOURCES})

# Conectar OpenCV
target_link_libraries(AILinkCapture PRIVATE ${OpenCV_LIBS})

# Librerias nativas de Windows
if(MSVC)
    # Aplicar optimizacion /O2 solo en Release para no chocar con /RTC1 en Debug
    target_compile_options(AILinkCapture PRIVATE $<$<CONFIG:Release>:/O2>)
endif()

