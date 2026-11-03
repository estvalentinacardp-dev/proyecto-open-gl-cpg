# YoutubeOpenGL

Aplicación de escritorio en C++ que renderiza un edificio de oficinas en formato glTF con OpenGL. El proyecto está preparado para abrirse y ejecutarse desde Visual Studio.

## Requisitos

- Windows 10/11.
- Visual Studio 2022 con la carga de trabajo **Desarrollo para el escritorio con C++** y herramientas para MSVC.
- SDK de Windows compatible con la solución.

Las dependencias de C++ y OpenGL usadas por el proyecto se encuentran en `Librerias/` (GLFW, GLAD, GLM, Dear ImGui y TinyGLTF). No es necesario descargarlas por separado si se conserva esa carpeta.

## Abrir, compilar y ejecutar

1. Abre `YoutubeOpenGL.slnx` en Visual Studio.
2. Selecciona la configuración **Debug** y la plataforma **x64**.
3. Compila con **Ctrl+Shift+B**.
4. Ejecuta con **F5** (depurador) o **Ctrl+F5** (sin depurador).

El directorio de trabajo de depuración está configurado como la carpeta del proyecto. Esto permite localizar el modelo con la ruta relativa `resources/atlanta_office/scene.gltf` al iniciar desde Visual Studio.

## Controles

- **W**, **A**, **S**, **D**: mover la cámara.
- **Mouse**: mirar alrededor.
- **Esc**: alternar entre capturar y liberar el cursor.

## Modelo glTF

El modelo está organizado en `resources/atlanta_office/`:

- `scene.gltf`: escena y materiales.
- `scene.bin`: datos geométricos binarios referenciados por la escena.
- `textures/`: texturas utilizadas por los materiales.
- `license.txt`: fuente, autor y condiciones de atribución.

Mantén juntos estos archivos y conserva sus rutas relativas para que TinyGLTF pueda resolver las referencias de la escena. La aplicación carga materiales de color base y emisión, incluida la transparencia cuando está definida; no representa todos los canales de un flujo PBR completo.

### Atribución del modelo

Este trabajo se basa en **"[FREE] Atlanta, Corperate Office Building"**, creado por [99.Miles](https://sketchfab.com/3d-models/free-atlanta-corperate-office-building-d96380fb001345cca9a9be121f3e43d5), bajo licencia [Creative Commons Attribution 4.0 (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/). Se permite el uso comercial con atribución. Consulta `resources/atlanta_office/license.txt` para el texto de crédito proporcionado con el recurso.

## Estructura principal

```text
YoutubeOpenGL.slnx       Solución de Visual Studio
YoutubeOpenGL.vcxproj    Configuración del proyecto C++
Main.cpp                 Inicialización, render y controles
GltfModel.cpp/.h         Carga y dibujo del modelo glTF
Librerias/               Dependencias incluidas
resources/atlanta_office/Modelo glTF y recursos asociados
```
