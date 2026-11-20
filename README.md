# YoutubeOpenGL

Aplicación de escritorio en C++ que renderiza un edificio de oficinas en formato glTF con OpenGL. El proyecto está preparado para abrirse y ejecutarse desde Visual Studio.

## Datos del proyecto

- **Nombre:** Valentina Cardozo Peña
- **Código:** 6014357

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

- **W/S**: acercar o alejar la cámara en modo libre.
- **A/D**: rodear el edificio en modo libre.
- **C**: salir del recorrido Bézier y activar la órbita manual.
- **V**: regresar al recorrido Bézier, continuando desde donde se pausó.
- **Mouse**: orbitar alrededor del edificio en modo libre.
- **Esc**: alternar entre capturar y liberar el cursor.

La cámara inicia recorriendo una curva Bézier cerrada alrededor del edificio. En ambos modos apunta al centro normalizado del modelo.

## Arquitectura

La aplicación separa la ventana, la cámara y el ciclo de render en `Main.cpp`, de la carga de la escena en `GltfModel.cpp` y `GltfModel.h`. TinyGLTF lee la escena, sus mallas y materiales; OpenGL crea los buffers, texturas y shaders; GLFW procesa las entradas y Dear ImGui muestra las instrucciones.

Durante la carga, la jerarquía y las matrices de transformación del modelo se aplanan en una lista de dibujo. Así se evitan recorridos recursivos y cálculos de matrices inversas en cada fotograma; después de copiar los recursos a la GPU, se libera la copia de CPU de TinyGLTF. El render reutiliza enlaces de texturas y estados de transparencia consecutivos.

En cada fotograma se calculan las matrices de **modelo** (normalización glTF y transformaciones de nodos), **vista** (cámara enfocada al edificio) y **proyección** (perspectiva). El shader transforma las normales con la matriz normal, calculada como la inversa transpuesta de la matriz de modelo. La iluminación Phong usa una luz puntual colocada en la cámara, con componentes ambiental, difusa y especular; la luz se actualiza al moverse la cámara.

```text
Main.cpp                 Ventana, controles, recorrido Bézier y matrices de cámara
GltfModel.cpp/.h         Lectura glTF, buffers, materiales y dibujo
shaders embebidos        Transformación de vértices e iluminación Phong
GLFW / GLAD / GLM        Ventana, funciones OpenGL y operaciones matemáticas
Dear ImGui               Panel de controles
TinyGLTF                 Lectura de escena y materiales glTF
resources/atlanta_office Modelo, geometría binaria, texturas y licencia
```

## Modelo glTF

El modelo está organizado en `resources/atlanta_office/`:

- `scene.gltf`: escena y materiales.
- `scene.bin`: datos geométricos binarios referenciados por la escena.
- `textures/`: texturas utilizadas por los materiales.
- `license.txt`: fuente, autor y condiciones de atribución.

Mantén juntos estos archivos y conserva sus rutas relativas para que TinyGLTF pueda resolver las referencias de la escena. La aplicación carga materiales de color base y emisión, incluida la transparencia cuando está definida; no representa todos los canales de un flujo PBR completo.

### Atribución del modelo

Este trabajo se basa en **"[FREE] Atlanta, Corperate Office Building"**, creado por [99.Miles](https://sketchfab.com/3d-models/free-atlanta-corperate-office-building-d96380fb001345cca9a9be121f3e43d5), bajo licencia [Creative Commons Attribution 4.0 (CC BY 4.0)](https://creativecommons.org/licenses/by/4.0/). Se permite el uso comercial con atribución. Consulta `resources/atlanta_office/license.txt` para el texto de crédito proporcionado con el recurso.
