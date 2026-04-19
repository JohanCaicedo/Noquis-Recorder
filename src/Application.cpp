#include "Application.hpp"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <opencv2/core/utils/logger.hpp>
#include <opencv2/imgproc.hpp>
#include <windows.h>

namespace {
Application* g_appInstance = nullptr;
WNDPROC g_oldWndProc = nullptr;
HWND g_hwnd = nullptr;
HMENU g_hMenu = nullptr;

std::string getExecutableDir() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t lastSlash = exePath.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        return exePath.substr(0, lastSlash);
    }
    return ".";
}

std::string getConfigPath() {
    return getExecutableDir() + "\\config.ini";
}

const char* getRTXQualityMenuLabel(int quality) {
    switch (quality) {
    case GPUUpscaler::kModeVsrBicubic: return "RTX Bicubic";
    case GPUUpscaler::kModeVsrLow: return "RTX VSR Low";
    case GPUUpscaler::kModeVsrMedium: return "RTX VSR Medium";
    case GPUUpscaler::kModeVsrHigh: return "RTX VSR High";
    case GPUUpscaler::kModeVsrUltra: return "RTX VSR Ultra";
    case GPUUpscaler::kModeHighBitrateLow: return "RTX HighBitrate Low";
    case GPUUpscaler::kModeHighBitrateMedium: return "RTX HighBitrate Medium";
    case GPUUpscaler::kModeHighBitrateHigh: return "RTX HighBitrate High";
    case GPUUpscaler::kModeHighBitrateUltra: return "RTX HighBitrate Ultra";
    default: return "RTX VSR Ultra";
    }
}

std::wstring toWide(const std::string& value) {
    if (value.empty()) return {};
    int required = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (required <= 1) {
        required = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
        if (required <= 1) return std::wstring(value.begin(), value.end());
        std::wstring wide(required - 1, L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, wide.data(), required);
        return wide;
    }
    std::wstring wide(required - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, wide.data(), required);
    return wide;
}

LRESULT CALLBACK CustomWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_COMMAND && g_appInstance) {
        g_appInstance->handleWin32Command(LOWORD(wParam));
    }
    return CallWindowProc(g_oldWndProc, hwnd, uMsg, wParam, lParam);
}
}
Application::Application(int deviceIndex)
    : deviceIndex(deviceIndex), isRunning(false), enableAA(false), enableDenoise(false), enableFrameGeneration(false), showFpsViewer(true), forceMjpg(true),
      currentAIType(AIType::NONE), yuy2AvailableForCurrentMode(false), appWindow("Visor"),
      pendingCaptureRestart(false), pendingAIInit(false), pendingDenoiseInit(false), pendingScreenshot(false) {
    devices = CaptureManager::getAvailableDevices();

    AppConfig cfg = ConfigManager::loadConfig(getConfigPath());
    if (cfg.loaded) {
        capWidth = cfg.capWidth;
        capHeight = cfg.capHeight;
        capFps = cfg.capFps;
        srWidth = cfg.srWidth;
        srHeight = cfg.srHeight;
        denoiseStrength = cfg.denoiseStrength;
        rtxQuality = cfg.rtxQuality;
        enableDenoise = cfg.enableDenoise;
        enableFrameGeneration = cfg.enableFrameGeneration;
        showFpsViewer = cfg.showFpsViewer;
        enableAA = cfg.enableAA;
        forceMjpg = cfg.forceMjpg;

        this->deviceIndex = -1;
        if (cfg.deviceHwIndex >= 0) {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (devices[i].hwIndex == cfg.deviceHwIndex) {
                    this->deviceIndex = static_cast<int>(i);
                    break;
                }
            }
        }
        if (this->deviceIndex < 0 && cfg.deviceIndex >= 0 && cfg.deviceIndex < (int)devices.size()) {
            this->deviceIndex = cfg.deviceIndex;
        }

        switch (cfg.aiType) {
        case static_cast<int>(AIType::NVIDIA_RTX): currentAIType = AIType::NVIDIA_RTX; break;
        case static_cast<int>(AIType::OPENCV_FSRCNN): currentAIType = AIType::OPENCV_FSRCNN; break;
        case static_cast<int>(AIType::SPATIAL_NEAREST): currentAIType = AIType::SPATIAL_NEAREST; break;
        case static_cast<int>(AIType::SPATIAL_BILINEAR): currentAIType = AIType::SPATIAL_BILINEAR; break;
        case static_cast<int>(AIType::SPATIAL_BICUBIC): currentAIType = AIType::SPATIAL_BICUBIC; break;
        case static_cast<int>(AIType::SPATIAL_LANCZOS4): currentAIType = AIType::SPATIAL_LANCZOS4; break;
        case static_cast<int>(AIType::SPATIAL_SHARP_BILINEAR): currentAIType = AIType::SPATIAL_SHARP_BILINEAR; break;
        case static_cast<int>(AIType::ANIME4K): currentAIType = AIType::ANIME4K; break;
        default: currentAIType = cfg.enableAI ? AIType::NVIDIA_RTX : AIType::NONE; break;
        }
    }

    if (this->deviceIndex < 0 && !devices.empty()) {
        this->deviceIndex = 0;
    }

    yuy2AvailableForCurrentMode = false;
    forceMjpg = true;
    g_appInstance = this;
}

Application::~Application() {
    videoProcessor.releaseFrameGenerator();
    videoProcessor.releaseDenoiser();
    videoProcessor.releaseUpscaler();
    captureManager.release();
    g_appInstance = nullptr;
}

void Application::applyVideoPreset(VideoModePreset preset) {
    switch (preset) {
    case VideoModePreset::P720_30:
        capWidth = 1280; capHeight = 720; capFps = 30;
        break;
    case VideoModePreset::P720_60:
        capWidth = 1280; capHeight = 720; capFps = 60;
        break;
    case VideoModePreset::P1080_30:
        capWidth = 1920; capHeight = 1080; capFps = 30;
        break;
    case VideoModePreset::P1080_60:
    default:
        capWidth = 1920; capHeight = 1080; capFps = 60;
        break;
    }
}

Application::VideoModePreset Application::getCurrentVideoPreset() const {
    if (capWidth == 1280 && capHeight == 720 && capFps == 30) return VideoModePreset::P720_30;
    if (capWidth == 1280 && capHeight == 720 && capFps == 60) return VideoModePreset::P720_60;
    if (capWidth == 1920 && capHeight == 1080 && capFps == 30) return VideoModePreset::P1080_30;
    return VideoModePreset::P1080_60;
}

void Application::refreshFormatAvailability() {
    // Modo conservador: mantenemos MJPEG como ruta estable por defecto.
    yuy2AvailableForCurrentMode = false;
    forceMjpg = true;
}

void Application::applyNegotiatedCaptureState() {
    const CaptureSessionInfo& session = captureManager.getSessionInfo();
    if (session.effectiveWidth > 0) capWidth = session.effectiveWidth;
    if (session.effectiveHeight > 0) capHeight = session.effectiveHeight;
    if (session.effectiveFps > 0) capFps = session.effectiveFps;

    if (session.effectiveFormat == CapturePixelFormat::MJPEG) {
        forceMjpg = true;
    } else if (session.effectiveFormat == CapturePixelFormat::YUY2) {
        forceMjpg = false;
    } else {
        forceMjpg = true;
    }

    refreshFormatAvailability();
}

void Application::updateWindowTitle() {
    if (!g_hwnd) return;

    CapturePixelFormat activeFormat = forceMjpg ? CapturePixelFormat::MJPEG : CapturePixelFormat::YUY2;
    const CaptureSessionInfo& session = captureManager.getSessionInfo();
    if (session.effectiveFormat != CapturePixelFormat::UNKNOWN) {
        activeFormat = session.effectiveFormat;
    }

    std::wstring title = L"Gnocchi's Viewer";
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        title += L" - " + toWide(devices[deviceIndex].name);
    }

    title += L" | ";
    title += toWide(std::to_string(capWidth) + "x" + std::to_string(capHeight));
    title += L" @ ";
    title += toWide(std::to_string(capFps));
    title += L" FPS | ";
    title += toWide(CaptureManager::pixelFormatToString(activeFormat));
    if (currentAIType == AIType::NVIDIA_RTX) {
        title += L" | ";
        title += toWide(getRTXQualityMenuLabel(rtxQuality));
    }

    SetWindowTextW(g_hwnd, title.c_str());
}

void Application::setupNativeMenu() {
    g_hwnd = FindWindowA(nullptr, "Visor");
    if (!g_hwnd) {
        std::cerr << "[Win32] No se encontro la ventana para inyectar el menu nativo." << std::endl;
        return;
    }

    g_hMenu = CreateMenu();

    HICON hIcon = LoadIconA(GetModuleHandle(nullptr), "IDI_ICON1");
    if (hIcon) {
        SendMessage(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessage(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    HMENU hMenuDevice = CreatePopupMenu();
    for (size_t i = 0; i < devices.size(); ++i) {
        AppendMenuA(hMenuDevice, MF_STRING, IDM_DEVICE_0 + (UINT)i, devices[i].name.c_str());
    }
    if (devices.empty()) AppendMenuA(hMenuDevice, MF_STRING | MF_GRAYED, 0, "Sin dispositivos");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuDevice, "Dispositivo");

    HMENU hMenuVideo = CreatePopupMenu();
    AppendMenuA(hMenuVideo, MF_STRING, IDM_VIDEO_720_30, "720p @ 30 FPS");
    AppendMenuA(hMenuVideo, MF_STRING, IDM_VIDEO_720_60, "720p @ 60 FPS");
    AppendMenuA(hMenuVideo, MF_STRING, IDM_VIDEO_1080_30, "1080p @ 30 FPS");
    AppendMenuA(hMenuVideo, MF_STRING, IDM_VIDEO_1080_60, "1080p @ 60 FPS");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuVideo, "Video");

    HMENU hMenuFormat = CreatePopupMenu();
    AppendMenuA(hMenuFormat, MF_STRING, IDM_FORMAT_MJPEG, "MJPEG");
    AppendMenuA(hMenuFormat, MF_STRING, IDM_FORMAT_YUY2, "YUY2");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuFormat, "Formato");

    HMENU hMenuAI = CreatePopupMenu();
    HMENU hMenuDenoise = CreatePopupMenu();
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_TOGGLE, "Activar Denoise");
    AppendMenuA(hMenuDenoise, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_00, "Denoise: Apagado");
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_05, "Denoise: Bajo");
    AppendMenuA(hMenuDenoise, MF_STRING, IDM_DENOISE_10, "Denoise: Alto");
    AppendMenuA(hMenuAI, MF_POPUP, (UINT_PTR)hMenuDenoise, "NVIDIA Denoise");
    AppendMenuA(hMenuAI, MF_STRING, IDM_FRAMEGEN_TOGGLE, "Frame Generation (Preview x2)");
    AppendMenuA(hMenuAI, MF_STRING, IDM_AA_TOGGLE, "AA Lanczos 4");

    HMENU hMenuUpscaling = CreatePopupMenu();
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_RTX_TOGGLE, "Activar IA: NVIDIA RTX VSR");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_FSRCNN_TOGGLE, "Activar IA: Universal FSRCNN");
    AppendMenuA(hMenuUpscaling, MF_SEPARATOR, 0, nullptr);
    HMENU hMenuRTXModes = CreatePopupMenu();
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_BICUBIC, "RTX Bicubic");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_LOW, "RTX VSR Low");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_MEDIUM, "RTX VSR Medium");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_HIGH, "RTX VSR High");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_ULTRA, "RTX VSR Ultra");
    AppendMenuA(hMenuRTXModes, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_HB_LOW, "RTX HighBitrate Low");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_HB_MEDIUM, "RTX HighBitrate Medium");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_HB_HIGH, "RTX HighBitrate High");
    AppendMenuA(hMenuRTXModes, MF_STRING, IDM_RTX_QUALITY_HB_ULTRA, "RTX HighBitrate Ultra");
    AppendMenuA(hMenuUpscaling, MF_POPUP, (UINT_PTR)hMenuRTXModes, "Modos RTX GPU");
    AppendMenuA(hMenuUpscaling, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_SPATIAL_NEAREST, "Escalado espacial: Nearest");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_SPATIAL_BILINEAR, "Escalado espacial: Bilinear");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_SPATIAL_BICUBIC, "Escalado espacial: Bicubic");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_SPATIAL_LANCZOS4, "Escalado espacial: Lanczos 4");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_SPATIAL_SHARP_BILINEAR, "Escalado espacial: Sharpened Bilinear");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_AI_ANIME4K_TOGGLE, "Anime4K");
    AppendMenuA(hMenuUpscaling, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_TARGET_1080, "Resolucion destino: 1080p");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_TARGET_1440, "Resolucion destino: 1440p");
    AppendMenuA(hMenuUpscaling, MF_STRING, IDM_TARGET_2160, "Resolucion destino: 4K");
    AppendMenuA(hMenuAI, MF_POPUP, (UINT_PTR)hMenuUpscaling, "Super Resolucion");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuAI, "Procesamiento IA");

    HMENU hMenuSettings = CreatePopupMenu();
    AppendMenuA(hMenuSettings, MF_STRING, IDM_SAVE_CONFIG, "Guardar Configuracion");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuSettings, "Opciones");

    HMENU hMenuTools = CreatePopupMenu();
    AppendMenuA(hMenuTools, MF_STRING, IDM_TAKE_SCREENSHOT, "Tomar Captura");
    AppendMenuA(hMenuTools, MF_STRING, IDM_FPS_VIEWER_TOGGLE, "Viewer de FPS");
    AppendMenuA(g_hMenu, MF_POPUP, (UINT_PTR)hMenuTools, "Herramientas");

    SetMenu(g_hwnd, g_hMenu);
    DrawMenuBar(g_hwnd);

    g_oldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)CustomWndProc);
    updateMenuChecks();
}

void Application::updateMenuChecks() {
    if (!g_hMenu) return;

    for (size_t i = 0; i < devices.size(); ++i) {
        CheckMenuItem(g_hMenu, static_cast<UINT>(IDM_DEVICE_0 + i), MF_BYCOMMAND | (deviceIndex == (int)i ? MF_CHECKED : MF_UNCHECKED));
    }

    VideoModePreset preset = getCurrentVideoPreset();
    CheckMenuItem(g_hMenu, IDM_VIDEO_720_30, MF_BYCOMMAND | (preset == VideoModePreset::P720_30 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_VIDEO_720_60, MF_BYCOMMAND | (preset == VideoModePreset::P720_60 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_VIDEO_1080_30, MF_BYCOMMAND | (preset == VideoModePreset::P1080_30 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_VIDEO_1080_60, MF_BYCOMMAND | (preset == VideoModePreset::P1080_60 ? MF_CHECKED : MF_UNCHECKED));

    CheckMenuItem(g_hMenu, IDM_FORMAT_MJPEG, MF_BYCOMMAND | (forceMjpg ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_FORMAT_YUY2, MF_BYCOMMAND | (!forceMjpg ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem(g_hMenu, IDM_FORMAT_YUY2, MF_BYCOMMAND | (yuy2AvailableForCurrentMode ? MF_ENABLED : MF_GRAYED));

    CheckMenuItem(g_hMenu, IDM_DENOISE_TOGGLE, MF_BYCOMMAND | (enableDenoise ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_DENOISE_00, MF_BYCOMMAND | (denoiseStrength == 0.0f ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_DENOISE_05, MF_BYCOMMAND | (denoiseStrength == 0.5f ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_DENOISE_10, MF_BYCOMMAND | (denoiseStrength == 1.0f ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_FRAMEGEN_TOGGLE, MF_BYCOMMAND | (enableFrameGeneration ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_RTX_TOGGLE, MF_BYCOMMAND | (currentAIType == AIType::NVIDIA_RTX ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_BICUBIC, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeVsrBicubic ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_LOW, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeVsrLow ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_MEDIUM, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeVsrMedium ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_HIGH, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeVsrHigh ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_ULTRA, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeVsrUltra ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_HB_LOW, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeHighBitrateLow ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_HB_MEDIUM, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeHighBitrateMedium ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_HB_HIGH, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeHighBitrateHigh ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_RTX_QUALITY_HB_ULTRA, MF_BYCOMMAND | (rtxQuality == GPUUpscaler::kModeHighBitrateUltra ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_FSRCNN_TOGGLE, MF_BYCOMMAND | (currentAIType == AIType::OPENCV_FSRCNN ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_SPATIAL_NEAREST, MF_BYCOMMAND | (currentAIType == AIType::SPATIAL_NEAREST ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_SPATIAL_BILINEAR, MF_BYCOMMAND | (currentAIType == AIType::SPATIAL_BILINEAR ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_SPATIAL_BICUBIC, MF_BYCOMMAND | (currentAIType == AIType::SPATIAL_BICUBIC ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_SPATIAL_LANCZOS4, MF_BYCOMMAND | (currentAIType == AIType::SPATIAL_LANCZOS4 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_SPATIAL_SHARP_BILINEAR, MF_BYCOMMAND | (currentAIType == AIType::SPATIAL_SHARP_BILINEAR ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AI_ANIME4K_TOGGLE, MF_BYCOMMAND | (currentAIType == AIType::ANIME4K ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_AA_TOGGLE, MF_BYCOMMAND | (enableAA ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_TARGET_1080, MF_BYCOMMAND | (srWidth == 1920 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_TARGET_1440, MF_BYCOMMAND | (srWidth == 2560 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_TARGET_2160, MF_BYCOMMAND | (srWidth == 3840 ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(g_hMenu, IDM_FPS_VIEWER_TOGGLE, MF_BYCOMMAND | (showFpsViewer ? MF_CHECKED : MF_UNCHECKED));
}

void Application::handleWin32Command(int menuId) {
    if (menuId >= IDM_DEVICE_0 && menuId < IDM_DEVICE_0 + 10) {
        int id = menuId - IDM_DEVICE_0;
        if (id < (int)devices.size()) {
            deviceIndex = id;
            refreshFormatAvailability();
            pendingCaptureRestart = true;
        }
    } else if (menuId == IDM_VIDEO_720_30) {
        applyVideoPreset(VideoModePreset::P720_30);
        refreshFormatAvailability();
        pendingCaptureRestart = true;
    } else if (menuId == IDM_VIDEO_720_60) {
        applyVideoPreset(VideoModePreset::P720_60);
        refreshFormatAvailability();
        pendingCaptureRestart = true;
    } else if (menuId == IDM_VIDEO_1080_30) {
        applyVideoPreset(VideoModePreset::P1080_30);
        refreshFormatAvailability();
        pendingCaptureRestart = true;
    } else if (menuId == IDM_VIDEO_1080_60) {
        applyVideoPreset(VideoModePreset::P1080_60);
        refreshFormatAvailability();
        pendingCaptureRestart = true;
    } else if (menuId == IDM_FORMAT_MJPEG) {
        forceMjpg = true;
        pendingCaptureRestart = true;
    } else if (menuId == IDM_FORMAT_YUY2 && yuy2AvailableForCurrentMode) {
        forceMjpg = false;
        pendingCaptureRestart = true;
    } else if (menuId == IDM_DENOISE_TOGGLE) {
        if (!enableDenoise) {
            pendingDenoiseInit = true;
        } else {
            enableDenoise = false;
            pendingDenoiseInit = false;
            videoProcessor.releaseDenoiser();
        }
    } else if (menuId == IDM_DENOISE_00) {
        denoiseStrength = 0.0f; enableDenoise = false; pendingDenoiseInit = false; videoProcessor.releaseDenoiser();
    } else if (menuId == IDM_DENOISE_05) {
        denoiseStrength = 0.5f; enableDenoise = false; videoProcessor.releaseDenoiser(); pendingDenoiseInit = true;
    } else if (menuId == IDM_DENOISE_10) {
        denoiseStrength = 1.0f; enableDenoise = false; videoProcessor.releaseDenoiser(); pendingDenoiseInit = true;
    } else if (menuId == IDM_FRAMEGEN_TOGGLE) {
        if (videoProcessor.isFrameGeneratorReady()) {
            enableFrameGeneration = !enableFrameGeneration;
            std::cout << "[FRUC] Frame Generation Preview x2: " << (enableFrameGeneration ? "ON" : "OFF") << std::endl;
        } else {
            enableFrameGeneration = false;
            std::cout << "[FRUC] La base del runtime no esta lista. No se puede activar Frame Generation Preview." << std::endl;
        }
    } else if (menuId == IDM_AA_TOGGLE) {
        enableAA = !enableAA;
    } else if (menuId == IDM_AI_RTX_TOGGLE) {
        if (currentAIType != AIType::NVIDIA_RTX) { currentAIType = AIType::NVIDIA_RTX; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_RTX_QUALITY_BICUBIC) {
        rtxQuality = GPUUpscaler::kModeVsrBicubic; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_LOW) {
        rtxQuality = GPUUpscaler::kModeVsrLow; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_MEDIUM) {
        rtxQuality = GPUUpscaler::kModeVsrMedium; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_HIGH) {
        rtxQuality = GPUUpscaler::kModeVsrHigh; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_ULTRA) {
        rtxQuality = GPUUpscaler::kModeVsrUltra; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_HB_LOW) {
        rtxQuality = GPUUpscaler::kModeHighBitrateLow; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_HB_MEDIUM) {
        rtxQuality = GPUUpscaler::kModeHighBitrateMedium; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_HB_HIGH) {
        rtxQuality = GPUUpscaler::kModeHighBitrateHigh; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_RTX_QUALITY_HB_ULTRA) {
        rtxQuality = GPUUpscaler::kModeHighBitrateUltra; scheduleRTXReinitIfActive();
    } else if (menuId == IDM_AI_FSRCNN_TOGGLE) {
        if (currentAIType != AIType::OPENCV_FSRCNN) { currentAIType = AIType::OPENCV_FSRCNN; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_AI_SPATIAL_NEAREST) {
        if (currentAIType != AIType::SPATIAL_NEAREST) { currentAIType = AIType::SPATIAL_NEAREST; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_AI_SPATIAL_BILINEAR) {
        if (currentAIType != AIType::SPATIAL_BILINEAR) { currentAIType = AIType::SPATIAL_BILINEAR; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_AI_SPATIAL_BICUBIC) {
        if (currentAIType != AIType::SPATIAL_BICUBIC) { currentAIType = AIType::SPATIAL_BICUBIC; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_AI_SPATIAL_LANCZOS4) {
        if (currentAIType != AIType::SPATIAL_LANCZOS4) { currentAIType = AIType::SPATIAL_LANCZOS4; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_AI_SPATIAL_SHARP_BILINEAR) {
        if (currentAIType != AIType::SPATIAL_SHARP_BILINEAR) { currentAIType = AIType::SPATIAL_SHARP_BILINEAR; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_AI_ANIME4K_TOGGLE) {
        if (currentAIType != AIType::ANIME4K) { currentAIType = AIType::ANIME4K; pendingAIInit = true; }
        else { currentAIType = AIType::NONE; videoProcessor.releaseUpscaler(); }
    } else if (menuId == IDM_TARGET_1080) {
        srWidth = 1920; srHeight = 1080; currentAIType = AIType::NONE; videoProcessor.releaseUpscaler();
    } else if (menuId == IDM_TARGET_1440) {
        srWidth = 2560; srHeight = 1440; currentAIType = AIType::NONE; videoProcessor.releaseUpscaler();
    } else if (menuId == IDM_TARGET_2160) {
        srWidth = 3840; srHeight = 2160; currentAIType = AIType::NONE; videoProcessor.releaseUpscaler();
    } else if (menuId == IDM_SAVE_CONFIG) {
        AppConfig cfg;
        cfg.deviceIndex = deviceIndex;
        cfg.deviceHwIndex = (deviceIndex >= 0 && deviceIndex < (int)devices.size()) ? devices[deviceIndex].hwIndex : -1;
        cfg.capWidth = capWidth;
        cfg.capHeight = capHeight;
        cfg.capFps = capFps;
        cfg.captureBackend = static_cast<int>(CaptureBackendType::OPENCV);
        cfg.srWidth = srWidth;
        cfg.srHeight = srHeight;
        cfg.denoiseStrength = denoiseStrength;
        cfg.aiType = static_cast<int>(currentAIType);
        cfg.rtxQuality = rtxQuality;
        cfg.enableDenoise = enableDenoise;
        cfg.enableAI = (currentAIType != AIType::NONE);
        cfg.enableFrameGeneration = enableFrameGeneration;
        cfg.showFpsViewer = showFpsViewer;
        cfg.enableAA = enableAA;
        cfg.forceMjpg = forceMjpg;
        ConfigManager::saveConfig(getConfigPath(), cfg);
        std::cout << "Configuracion guardada en " << getConfigPath() << " via Menu Nativo." << std::endl;
    } else if (menuId == IDM_TAKE_SCREENSHOT) {
        pendingScreenshot = true;
    } else if (menuId == IDM_FPS_VIEWER_TOGGLE) {
        showFpsViewer = !showFpsViewer;
    }

    updateMenuChecks();
}

void Application::run() {
    isRunning = true;
    cv::Mat frame, processedFrame;
    double measuredViewerFps = 0.0;
    int displayedFrames = 0;
    auto lastFpsSample = std::chrono::steady_clock::now();

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    appWindow.show(cv::Mat(720, 1280, CV_8UC3, cv::Scalar(0, 0, 0)));
    cv::waitKey(10);

    setupNativeMenu();
    updateMenuChecks();

    bool captureActive = false;
    if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
        captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight, capFps, forceMjpg, CaptureBackendType::OPENCV);
        if (captureActive) {
            applyNegotiatedCaptureState();
            updateWindowTitle();
            audioManager.start(devices[deviceIndex].name);
            videoProcessor.initFrameGenerator(capWidth, capHeight);
            if (currentAIType != AIType::NONE) pendingAIInit = true;
            if (enableDenoise) pendingDenoiseInit = true;
        }
    }

    std::cout << "Controles: [ESC] Zen Mode | Selecciona opciones en la barra Win32 superior." << std::endl;

    while (isRunning) {
        if (pendingCaptureRestart) {
            std::cout << "Reiniciando captura de video..." << std::endl;
            captureManager.release();
            audioManager.stop();
            AIType oldAI = currentAIType;
            bool oldDenoise = enableDenoise;
            currentAIType = AIType::NONE;
            enableDenoise = false;
            videoProcessor.releaseUpscaler();
            videoProcessor.releaseDenoiser();
            videoProcessor.releaseFrameGenerator();

            if (deviceIndex >= 0 && deviceIndex < (int)devices.size()) {
                captureActive = captureManager.initialize(devices[deviceIndex].hwIndex, capWidth, capHeight, capFps, forceMjpg, CaptureBackendType::OPENCV);
                if (captureActive) {
                    applyNegotiatedCaptureState();
                    updateWindowTitle();
                    audioManager.start(devices[deviceIndex].name);
                    videoProcessor.initFrameGenerator(capWidth, capHeight);
                    if (oldAI != AIType::NONE) pendingAIInit = true;
                    if (oldDenoise) pendingDenoiseInit = true;
                }
            }
            pendingCaptureRestart = false;
            updateMenuChecks();
        }

        if (pendingAIInit) {
            std::cout << "[App] Inicializando modelo SuperResolucion IA: " << (int)currentAIType << " ..." << std::endl;
            if (!videoProcessor.initUpscaler(capWidth, capHeight, srWidth, srHeight, currentAIType, rtxQuality)) {
                std::cout << "[App] Upscaler Fallo! Apagando flag." << std::endl;
                currentAIType = AIType::NONE;
            }
            pendingAIInit = false;
            updateWindowTitle();
            updateMenuChecks();
        }

        if (pendingDenoiseInit) {
            std::cout << "Inicializando Denoise..." << std::endl;
            bool ready = videoProcessor.isDenoiserReady();
            if (!ready) ready = videoProcessor.initDenoiser(capWidth, capHeight, denoiseStrength);
            enableDenoise = ready;
            pendingDenoiseInit = false;
            updateMenuChecks();
        }

        if (captureActive) {
            if (!captureManager.getNextFrame(frame)) {
                frame = cv::Mat(capHeight, capWidth, CV_8UC3, cv::Scalar(0, 0, 0));
            }
        } else {
            frame = cv::Mat(720, 1280, CV_8UC3, cv::Scalar(20, 20, 20));
        }

        auto start = std::chrono::high_resolution_clock::now();
        videoProcessor.processFrame(frame, processedFrame, currentAIType, enableDenoise && captureActive);
        auto end = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(end - start).count();

        static std::ofstream profilerLog("capturadora_rendimiento.log");
        if ((currentAIType != AIType::NONE || enableDenoise || enableFrameGeneration) && captureActive) {
            profilerLog << "[Profiler] Pipeline IA tomo: " << elapsedMs
                        << " ms | Denoise: " << (enableDenoise ? "ON" : "OFF")
                        << " | FG Preview: " << (enableFrameGeneration ? "ON" : "OFF")
                        << " | Modo (" << (int)currentAIType << ")" << std::endl;
        }

        cv::Mat displayFrame = processedFrame;
        if (enableAA && (displayFrame.cols != srWidth || displayFrame.rows != srHeight)) {
            cv::Mat aaFrame;
            cv::resize(displayFrame, aaFrame, cv::Size(srWidth, srHeight), 0, 0, cv::INTER_LANCZOS4);
            displayFrame = aaFrame;
        }

        if (pendingScreenshot) {
            CreateDirectoryA("capturas", nullptr);
            auto t = std::time(nullptr);
            std::tm tm = {};
            localtime_s(&tm, &t);
            char buf[64];
            std::strftime(buf, sizeof(buf), "capturas\\captura_%Y%m%d_%H%M%S.png", &tm);
            cv::imwrite(buf, displayFrame);
            std::cout << "[App] Captura guardada en: " << buf << std::endl;
            pendingScreenshot = false;
        }

        ++displayedFrames;
        auto now = std::chrono::steady_clock::now();
        double fpsWindowSeconds = std::chrono::duration<double>(now - lastFpsSample).count();
        if (fpsWindowSeconds >= 0.5) {
            measuredViewerFps = displayedFrames / fpsWindowSeconds;
            displayedFrames = 0;
            lastFpsSample = now;
        }

        const double fgPreviewFps = enableFrameGeneration ? (measuredViewerFps * 2.0) : measuredViewerFps;
        const double reportedDisplayFps = enableFrameGeneration ? fgPreviewFps : measuredViewerFps;

        const CaptureSessionInfo& session = captureManager.getSessionInfo();
        int realInputWidth = session.effectiveWidth > 0 ? session.effectiveWidth : frame.cols;
        int realInputHeight = session.effectiveHeight > 0 ? session.effectiveHeight : frame.rows;

        std::string meterLine1 = enableFrameGeneration
            ? "Viewer FPS salida: " + std::to_string((int)std::lround(reportedDisplayFps))
            : "Viewer FPS real: " + std::to_string((int)std::lround(measuredViewerFps));
        std::string meterLine2 = enableFrameGeneration
            ? "Base real sin FG: " + std::to_string((int)std::lround(measuredViewerFps))
            : "FG Preview x2: OFF";
        std::string meterLine3 = "Entrada real: " + std::to_string(realInputWidth) + "x" + std::to_string(realInputHeight);
        std::string meterLine4 = "Salida mostrada: " + std::to_string(displayFrame.cols) + "x" + std::to_string(displayFrame.rows);

        if (showFpsViewer) {
            cv::rectangle(displayFrame, cv::Rect(12, 12, 380, 104), cv::Scalar(0, 0, 0), cv::FILLED);
            cv::putText(displayFrame, meterLine1, cv::Point(22, 34), cv::FONT_HERSHEY_SIMPLEX, 0.58, enableFrameGeneration ? cv::Scalar(0, 215, 255) : cv::Scalar(0, 255, 0), 1, cv::LINE_AA);
            cv::putText(displayFrame, meterLine2, cv::Point(22, 58), cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(180, 180, 180), 1, cv::LINE_AA);
            cv::putText(displayFrame, meterLine3, cv::Point(22, 82), cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            cv::putText(displayFrame, meterLine4, cv::Point(22, 106), cv::FONT_HERSHEY_SIMPLEX, 0.58, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        }

        if (appWindow.isWindowFullscreen()) {
            if (GetMenu(g_hwnd) != nullptr) SetMenu(g_hwnd, nullptr);
            int w = displayFrame.cols;
            int h = displayFrame.rows;
            int targetH = (w * 10) / 16;
            if (targetH > h) {
                int pad = (targetH - h) / 2;
                cv::copyMakeBorder(displayFrame, displayFrame, pad, pad, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
            }
        } else if (GetMenu(g_hwnd) == nullptr) {
            SetMenu(g_hwnd, g_hMenu);
            DrawMenuBar(g_hwnd);
        }

        appWindow.show(displayFrame);
        handleInput();

        if (cv::getWindowProperty(appWindow.getName(), cv::WND_PROP_VISIBLE) < 1) {
            isRunning = false;
            break;
        }
    }

    if (g_oldWndProc && g_hwnd) {
        SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
    }

    std::cout << "Recursos liberados. Programa terminado." << std::endl;
}

void Application::handleInput() {
    char key = (char)cv::waitKey(1);
    if (key == 27) appWindow.toggleFullscreen();
    else if (key == 'q' || key == 'Q') isRunning = false;
}

void Application::scheduleRTXReinitIfActive() {
    if (currentAIType == AIType::NVIDIA_RTX) {
        videoProcessor.releaseUpscaler();
        pendingAIInit = true;
    }
    updateWindowTitle();
}
