# 😸 Gnocchi's Viewer

> [!NOTE]
> **Notas del diseñador:** Este es un programa creado por, como quien dice, un diseñador. Soy desarrollador web y diseñador web, pero el crear apps para Windows se sale de mis conocimientos. Por eso que la mayoría del código está hecho con *vibe coding*. Mi intención no es aparentar que soy desarrollador de Windows, sino compartir una herramienta que me parece interesante y necesaria si, como yo, tienes una tarjeta capturadora USB 2.0 y quieres una herramienta directa que intenta usar filtros para mejorar la imagen y con la minima latecia posible.

Visor de capturadoras en C++ para Windows con enfoque en baja latencia, limpieza de artefactos y escalado por IA.

## Que hace

- Captura video por `OpenCV + MSMF`.
- Prioriza un flujo de baja latencia sin colas ni buffering extra.
- Puede aplicar limpieza de artefactos con NVIDIA Video Effects.
- Puede escalar con `NVIDIA RTX VSR`, `FSRCNN`, `Anime4K` (ACNet) o escaladores espaciales ligeros.
- Base de `Frame Generation` integrada con `NVIDIA Optical Flow SDK` (Preview x2).
- Incluye puente de audio directo por `miniaudio`.
- Usa una ventana OpenCV con menu nativo Win32, sin frameworks GUI pesados.

## Flujo principal

1. Ingesta desde la capturadora por `CAP_MSMF`.
2. Frame en RAM.
3. Procesamiento opcional:
   - `NVIDIA Denoise`
   - `Frame Generation (Preview x2)`
   - `NVIDIA RTX VSR`
   - `FSRCNN`
   - `Anime4K`
   - Escaladores espaciales: `Nearest`, `Bilinear`, `Bicubic`, `Lanczos 4`, `Sharpened Bilinear`.
4. Presentacion inmediata en ventana.

Cuando se usa `Denoise + RTX VSR`, el proyecto mantiene el encadenamiento entre filtros en VRAM antes de bajar el frame final para mostrarlo.

## Controles y menu

- `ESC`: alterna Zen Mode / fullscreen.
- `Q`: cierra la aplicacion.
- `Dispositivo`: selecciona la capturadora.
- `Ingesta (USB)`: cambia resolucion, FPS y MJPEG.
- `Procesamiento IA`: activa Denoise, Frame Generation (Preview), RTX VSR, FSRCNN, Anime4K y escaladores espaciales.
- `Herramientas`: toma capturas de pantalla y activa el visor de FPS.
- `Opciones > Guardar Configuracion`: guarda el estado actual en `config.ini`.

## Requisitos

- Windows.
- Visual Studio Build Tools o Visual Studio con toolchain C++.
- CMake.
- vcpkg.
- CUDA Toolkit.
- Drivers NVIDIA compatibles para usar los filtros RTX.
- Carpeta `NVIDIA Video Effects` presente en la raiz del proyecto.

## Estructura esperada del SDK NVIDIA

El proyecto espera esta raiz:

```text
G:\dev\Capturadora\NVIDIA Video Effects
```

Y usa estas rutas en tiempo de ejecucion:

```text
NVIDIA Video Effects\bin
NVIDIA Video Effects\features\nvvfxvideosuperres\bin
NVIDIA Video Effects\models
```

## Notas de arquitectura

- No introducir buffering adicional en video o audio.
- `GPUUpscaler` trabaja sobre buffers RGBA.
- `GPUDenoiser` trabaja dentro del flujo NVIDIA VFX esperado por el proyecto.
- No remover la manipulacion dinamica de `PATH` en los modulos NVIDIA.
- Toda reserva GPU debe liberar sus recursos en `release()`.

## Estado actual

- Persistencia de configuracion funcionando para todos los modos y herramientas.
- Base de `Frame Generation` con NVIDIA Optical Flow integrada (fase de preview).
- Integración de `Anime4KCPP` para contenido tipo anime/2D.
- Soporte para múltiples escaladores espaciales ligeros (estilo Lossless Scaling).
- Visor de FPS opcional y persistente en el menú de Herramientas.
- Emparejamiento de audio mejorado usando el nombre real de la capturadora.

## Capturas

Las capturas se guardan en:

```text
G:\dev\Capturadora\capturas
```
