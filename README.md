# 🍝 Gnocchi's Viewer
### *El visor de capturadoras definitivo impulsado por Inteligencia Artificial*

**Gnocchi's Viewer** es un software de alto rendimiento desarrollado en C++ diseñado para transformar capturadoras de video económicas (USB 2.0 y 3.0) en dispositivos de visualización de grado profesional. Utilizando la potencia de los **Tensor Cores de NVIDIA** y modelos neurales universales, elimina el ruido, escala la imagen y reduce la latencia a niveles imperceptibles.

---

## 🚀 Guía de Inicio Rápido

### 1. Descarga y Preparación
Simplemente descarga la **última versión (Latest Release)** de la aplicación desde el repositorio. 
> [!NOTE]
> No necesitas instalar dependencias externas; todos los núcleos de NVIDIA, modelos de IA y librerías de procesamiento vienen pre-instalados dentro de la carpeta del programa.

### 2. Paciencia al Iniciar
Al abrir el programa, verás que tarda unos segundos en mostrar la imagen. **Esto es normal**. El software está realizando un escaneo profundo de todos tus dispositivos de video y audio para garantizar la mejor sincronización posible. Una vez detectada la interfaz, el programa funcionará de forma fluida.

---

## 🛠️ Cómo Utilizar el Programa

Para obtener la mejor experiencia, sigue este orden de configuración:

### A. Selección de Dispositivo
Dirígete al menú **"Dispositivo"** en la parte superior y selecciona la capturadora que deseas utilizar. El programa se reiniciará automáticamente para conectar el motor de latencia ultra-baja al hardware seleccionado.

### B. Configuración de Ingesta (USB)
En el menú **"Ingesta (USB)"**, puedes ajustar cómo el programa recibe los datos de la capturadora:
- **Resolución**: Elige entre 1080p o 720p según la capacidad de tu hardware.
- **Frame Rate**: Cambia entre 60 FPS o 30 FPS.
- **Forzar Ingesta MJPEG**: 
    - **Si tienes USB 3.0**: Recomendamos **desmarcar** esta opción para usar formatos sin compresión (RAW) y obtener la máxima fidelidad.
    - **Si tienes USB 2.0**: Mantén esta opción **marcada** (el check activo) para asegurar una visualización fluida de 60 FPS dentro de los límites de ancho de banda.

### C. Procesamiento IA (Inteligencia Artificial)
Este es el corazón de **Gnocchi's Viewer**. En el menú **"Procesamiento IA"** encontrarás:

1. **NVIDIA Denoise**: 
    - Selecciona el nivel (Bajo o Alto) y haz clic en **"Activar Denoise"**.
    - Utiliza tecnología de NVIDIA para limpiar el ruido y los "artefactos" de compresión de las capturadoras baratas, dejando una imagen cristalina.
2. **AA Lanczos 4**: Un filtro de anti-aliasing matemático de alta calidad para suavizar bordes.
3. **Super Resolución**:
    - **Resolución Destino**: Elige si quieres ver tu juego en 1080p, 1440p o **4K**.
    - **NVIDIA RTX VSR**: Si tienes una tarjeta NVIDIA RTX (serie 30 o superior), esta opción usará los Tensor Cores para reescalar el video con calidad cinematográfica.
    - **Universal FSRCNN**: Si no tienes una tarjeta NVIDIA RTX, esta red neuronal universal procesará la imagen mediante CPU/OpenCL para mejorar la definición.

### D. Guardar Configuración
No pierdas tiempo configurando todo cada vez que abras el programa. 
- Ve a **"Opciones" > "Guardar Configuración"**. 
- El software recordará tu dispositivo, tus filtros de IA y tus ajustes de ingesta para que siempre inicie exactamente como a ti te gusta.

---

## 📸 Herramientas y Atajos

- **F11 / ESC**: Activa el **Zen Mode** (Pantalla completa sin bordes y oculta automáticamente los menús para una inmersión total).
- **Captura de Pantalla**: En el menú **"Herramientas"**, puedes tomar un screenshot instantáneo de lo que estás viendo. Las capturas se guardan en la carpeta `/capturas`.

---

## 🧠 Especificaciones Técnicas (Para Developers)

- **Zero-Buffer Logic**: Pipeline de video modificado para inyectar frames directamente desde el hardware a VRAM, logrando una latencia <10ms.
- **NVIDIA Video Effects SDK**: Integración nativa para Artifact Reduction y Super Resolution.
- **OpenCV DNN**: Motor de inferencia para modelos FSRCNN (.pb) universales.
- **C++ 17 & Win32 API**: Interfaz de usuario de bajo nivel para evitar el overhead de frameworks pesados.

---

> [!IMPORTANT]
> **Hecho por y para gamers**. Este visor está diseñado para que jugar a través de una capturadora se sienta tan natural como si estuvieras conectado directamente al monitor.
