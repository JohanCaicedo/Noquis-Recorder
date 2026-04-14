#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "AudioManager.hpp"
#include <iostream>
#include <string>

// Callback de audio de muy baja latencia, recoge el input (capturadora) y lo pone en el output (audifonos)
void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    if (pInput != NULL && pOutput != NULL) {
        // En copy simple, pasamos los datos flotantes de canal 2 (Stereo)
        const float* pIn = (const float*)pInput;
        float* pOut = (float*)pOutput;
        ma_uint32 channels = pDevice->playback.channels;
        for (ma_uint32 i = 0; i < frameCount * channels; ++i) {
            pOut[i] = pIn[i];
        }
    }
}

AudioManager::AudioManager() : miniaudioDevice(nullptr), isRunning(false) {
}

AudioManager::~AudioManager() {
    stop();
}

bool AudioManager::start() {
    if (isRunning) return true;

    ma_context context;
    if (ma_context_init(NULL, 0, NULL, &context) != MA_SUCCESS) {
        std::cerr << "[Audio] Fallo al inicializar contexto de audio." << std::endl;
        return false;
    }

    ma_device_info* pPlaybackInfos;
    ma_uint32 playbackCount;
    ma_device_info* pCaptureInfos;
    ma_uint32 captureCount;
    
    if (ma_context_get_devices(&context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        ma_context_uninit(&context);
        return false;
    }

    ma_device_id targetCaptureId;
    bool found = false;
    
    // Buscar un dispositivo que suene a capturadora
    for (ma_uint32 i = 0; i < captureCount; ++i) {
        std::string name = pCaptureInfos[i].name;
        if (name.find("USB") != std::string::npos || name.find("Digital") != std::string::npos || name.find("FHD") != std::string::npos || name.find("Capture") != std::string::npos) {
            std::cout << "[Audio] Detectada capturadora en hardware sonoro: " << name << std::endl;
            targetCaptureId = pCaptureInfos[i].id;
            found = true;
            break; // Tomar la primera coincidencia
        }
    }

    ma_context_uninit(&context);

    // Preparar configuracion Full Duplex (Capturar del microfono de la USB, y enviar al parlante)
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);
    config.capture.pDeviceID  = found ? &targetCaptureId : NULL; 
    config.capture.format     = ma_format_f32;
    config.capture.channels   = 2;
    config.playback.pDeviceID = NULL; // Dispositivo de salida por defecto de Windows
    config.playback.format    = ma_format_f32;
    config.playback.channels  = 2;
    config.sampleRate         = 48000;
    config.dataCallback       = data_callback;
    config.periodSizeInFrames = 480; // 10ms de latencia teórica (Muy bajo)

    ma_device* pDevice = new ma_device;
    if (ma_device_init(NULL, &config, pDevice) != MA_SUCCESS) {
        std::cerr << "[Audio] Error inicializando el puente de audio." << std::endl;
        delete pDevice;
        return false;
    }

    if (ma_device_start(pDevice) != MA_SUCCESS) {
        std::cerr << "[Audio] Error arrancando motor de audio." << std::endl;
        ma_device_uninit(pDevice);
        delete pDevice;
        return false;
    }

    miniaudioDevice = pDevice;
    isRunning = true;
    std::cout << "[Audio] Engine de Latencia Ultra-Baja activo y funcionando." << std::endl;
    return true;
}

void AudioManager::stop() {
    if (isRunning && miniaudioDevice) {
        ma_device* pDevice = (ma_device*)miniaudioDevice;
        ma_device_uninit(pDevice);
        delete pDevice;
        miniaudioDevice = nullptr;
        isRunning = false;
        std::cout << "[Audio] Engine detenido." << std::endl;
    }
}
