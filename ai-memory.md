# AI-Link Capture (Ñoquis Viwer) - System Memory & Architecture

Este documento almacena todo el contexto y estado vital de la arquitectura de "AILinkCapture". Está diseñado para servir de "memoria a largo plazo" e instrucciones de core a los agentes IA que interactúen con el código del proyecto.

## 1. Visión General del Proyecto
AI-Link Capture es una plataforma de puenteo multimedia hiper-optimizada en C++ diseñada para la ingesta y manipulación de capturadoras de hardware barato (USB 2.0 / USB 3.0). El objetivo general del software es adquirir video bruto y tratarlo con Inteligencia Artificial utilizando tensores acelerados en las GPUs de la familia RTX de NVIDIA, resultando en una imagen visualmente perfecta sin causar latencia de latido ("lag") perceptible al jugar.

## 2. Componentes Arquitectónicos

El proyecto sigue una metodología dividida en Managers altamente acoplados pero funcionales independientes:

* **CaptureManager (`src/CaptureManager`)**:
  - Manipula la cámara o tarjeta física utilizando el Backend de `CV_CAP_MSMF` (Media Foundation).
  - Por defecto obliga a usar hardware comprimiendo el bufer a Size `1` (Zero-Buffer mode) erradicando la latencia crónica de las colas FIFO de OpenCV.
  - Ofrece alternancia de códecs (MJPEG vs Uncompressed RAW YUYV) lo cual es crítico: las USB 2.0 requieren Forzar MJPEG (de lo contrario van a 5 FPS por cuello de bus USB), pero las capturadoras USB 3.0 pueden desactivarlo para menor latencia.

* **El Pipeline de Inteligencia Artificial (NVIDIA Video Effects SDK)**:
  - **`GPUDenoiser`**: Originalmente planeado como Denoiser Temporal, refactorizado a Spatial Artifact Reduction. Corre asíncronamente en CUDA y extrae macro-bloqueos en la ingesta MJPEG que ensucian la pantalla por compresión USB.
  - **`GPUUpscaler`**: Utiliza el filtro RTX Video Super Resolution (VSR). Toma una resolución de ingreso subyacente y genera una matriz virtual súper ampliada reconstruyendo las orillas (e.g., convirtiendo 1080p a un 4K nativo deslumbrante).

* **Zero-Copy VRAM Workflow (`VideoProcessor`)**:
  - Para alcanzar la exigencia imperativa de <16.6ms (es decir, evitar el lag para sostener 60 FPS fijos), los SDKs de GPU se comunican entre ellos en VRAM.
  - **GPUDenoiser** aplica `denoiseToGPU()`, procesando la señal y manteniendo su estado nativo pasivo en la memoria de la tarjeta gráfica (VRAM) sin descender a memoria RAM por el puente PCIe.
  - El **GPUUpscaler** llama al método `upscaleFromGPU()` asimilando en su ingesta el apuntador de la IA limpiadora como entrada de Hardware inmediata, procesando a 4K y finalizando el descenso de ida a MS Windows con la salida purificada.

* **Application (`Application.cpp/hpp`) & AppWindow**:
  - OpenCV Window genera una GUI por defecto a prueba de fallos interna. Se le conoce a la entidad de la ventana explícita bajo "Ñoquis Viwer" (Previamente "Visor" o "AI-Link Capture").
  - Un Win32 Hook (`CustomWndProc`) se adhiere nativamente al Handle gráfico principal de MS Windows e impone una elegante barra de Herrramientas de Opciones limpiando las impurezas de ImGui o primitivos rudimentarios como Overlay. 
  - Al presionar (F11/ESC) o `Fullscreen`, la aplicación se esconde `SetMenu(hwnd, NULL)`, garantizando inmersión pura (Zen Mode).

* **ConfigManager & Persistencia**:
  - Transacciones se asientan sin demoras en `config.ini` parseando de ASCII a Integers y viceversa. Mapea flags como `enableAA` (Lanczos 4 Interpolar anti-aliasing) o los interruptores `forceMjpg`.

## 3. Comandos de Compilación & Despliegue
  
- Nunca se emplean construcciones dinámicas experimentales.
- El build system usa el Toolchain nativo inyectado en Visual Studio y MSBuild mediante VCPKG para C++ 17.
- Siempre compilar a `Release` para obtener la optimización /O2.
- Comando: `cmake --build build --config Release`
 
## 4. Notas Claves de Desarrollo (Trampas Comunes)

- **Codificación de Cadenas Visuales**: Componentes nativos como MSVC Resource Compiler `.RC` explotan en errores de sintaxis si detectan caracteres de tildes o eñes, y Win32 API requiere Wide Strings en Universal Escaping (ej: `L"\u00D1"`).
- **Destrucción de Hilos por Bucle**: Nunca dependas completamente del bucle principal de OpenCV por defecto. La inyección de DLLs altera Windows IPC, en caso de salir interceptando nativamente `WM_CLOSE`, vigila la caída de visibilidad (`cv::WND_PROP_VISIBLE < 1`) y propaga la bandera `isRunning = false` de forma sincrónica.
- **Ruta de Modelos IA**: Si una instancia de IA se levanta con Error y la consola marca: `The file could not be found`, quiere verificar que tu entorno local tiene el Path `NVIDIA Video Effects\bin` o copia de DLLs contigua al ejecutable compilado.

## Resumen del Progreso Reciente

- [Refactorizado] El motor ahora no satura el ancho de PCI-e realizando un traspaso en C++ limpio de GPU A GPU.
- [Añadido] Telemetría cronométrica pura en C++ `capturadora_rendimiento.log` ha destituido con eficiencia los registros ciegos NVCV.
- [Añadido] Habilidad Nativa OpenCV-Win32 captadora para tomar instantáneas perfectas `(Screenshots)` desde el marco del GPU salvados en `$app/capturas`.
- [Mejorado] Arquitectura Visual UI con Inmersión total (Hiding Menus y Fullscreen mode) pulido bajo el nuevo codename `Ñoquis Viwer`.
