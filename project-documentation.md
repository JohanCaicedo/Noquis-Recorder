# Especificación Técnica: AI-Link Capture

## 1. Objetivo del Proyecto
Desarrollar una aplicación de visualización de video de latencia ultra baja para consolas, diseñada para transformar una señal comprimida de baja calidad (USB 2.0) en una imagen de alta fidelidad mediante reconstrucción por inteligencia arificial NVIDIA.

## 2. Tecnologías y Herramientas
* **Lenguaje:** C++17 (Optimización de memoria y velocidad nativa).
* **Captura:** OpenCV con Backend DirectShow (Acceso directo al hardware en Windows).
* **Procesamiento:** NVIDIA CUDA / TensorRT (Para ejecutar la IA en los núcleos de la GPU).
* **Gestión:** CMake y vcpkg (Automatización de dependencias).

---

## 3. Arquitectura del Pipeline (El "Momento" de la IA)

El escalado con IA ocurre en la **Etapa de Post-procesamiento**. Este es el flujo exacto que sigue cada frame de video:

1.  **Ingesta (Capture):** El programa extrae el frame comprimido en formato MJPEG desde el puerto USB.
2.  **Conversión (Decoding):** El frame se descomprime y se transforma en un formato que la tarjeta de video pueda entender (RGB/YUV).
3.  **Transferencia a VRAM:** El frame se envía de la memoria RAM del sistema a la memoria de la tarjeta de video (GPU).
4.  **ETAPA DE IA (Inferencia):** * **Aquí es donde entra la IA.** El procesador de la tarjeta de video toma el frame de 1080p y, mediante una red neuronal, "reconstruye" los píxeles perdidos por la compresión y escala la imagen.
    * Este proceso es instantáneo gracias a los núcleos dedicados de las GPUs modernas.
5.  **Renderizado (Display):** La imagen ya mejorada se dibuja directamente en la ventana de la aplicación.



---

## 4. Funcionamiento y Características
* **Modo Zen (Interfaz Invisible):** El programa inicia con una ventana de visualización pura. Los controles de configuración solo son visibles bajo demanda para no distraer del juego.
* **Zero-Buffer Logic:** A diferencia de programas de grabación (como OBS), este programa no guarda video en el disco duro, lo que elimina el retraso (lag) de procesamiento.
* **Escalado Inteligente:** Utiliza algoritmos de super-resolución que analizan los bordes de los objetos en el juego (como los personajes de la Switch) para evitar que se vean borrosos.

## 5. Controles de Usuario
* **Alternar Pantalla Completa:** Se utiliza la tecla `ESC` para ocultar instantáneamente todos los elementos de Windows (bordes, barra de tareas) y centrar el juego.
* **Activación de IA:** Un interruptor rápido (tecla `F`) permite comparar en tiempo real la imagen original "sucia" del USB contra la imagen procesada por la IA.
* **Cierre Seguro:** Salida inmediata del programa liberando los recursos de la capturadora para evitar bloqueos del dispositivo.

---

## 6. Hitos de Desarrollo
1.  Configuración del entorno de compilación C++.
2.  Implementación de la captura estable a 60 FPS.
3.  Desarrollo del sistema de ventana "Borderless Fullscreen".
4.  Integración del filtro de reconstrucción acelerado por GPU.

# 1. Configurar el proyecto (detecta los cambios en CMakeLists y la ruta del SDK)
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=g:\dev\vcpkg\scripts\buildsystems\vcpkg.cmake

# 2. Compilar la aplicación en modo Release
cmake --build build --config Release
