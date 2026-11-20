#include <cmath>
#include <iostream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "GltfModel.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace {
constexpr glm::vec3 kFocus(0.0f, 0.0f, 0.0f);
constexpr glm::vec3 kUp(0.0f, 1.0f, 0.0f);
glm::vec3 cameraPosition(0.0f, 1.2f, 6.0f);
bool cameraOnBezier = true;
bool mouseCaptured = true;
bool firstMouse = true;
float lastX = 500.0f, lastY = 400.0f;
float pathProgress = 0.0f;
float freeYaw = 0.0f, freePitch = 0.1974f, freeRadius = 6.12f;

glm::vec3 bezier(const glm::vec3& p0, const glm::vec3& p1,
                const glm::vec3& p2, const glm::vec3& p3, float t)
{
    const float u = 1.0f - t;
    return u*u*u*p0 + 3.0f*u*u*t*p1 + 3.0f*u*t*t*p2 + t*t*t*p3;
}

glm::vec3 bezierCameraPosition(float progress)
{
    constexpr float k = 3.3137085f;
    constexpr float y = 1.2f;
    static const glm::vec3 p[4][4] = {
        {{0,y,6}, {k,y,6}, {6,y,k}, {6,y,0}},
        {{6,y,0}, {6,y,-k}, {k,y,-6}, {0,y,-6}},
        {{0,y,-6}, {-k,y,-6}, {-6,y,-k}, {-6,y,0}},
        {{-6,y,0}, {-6,y,k}, {-k,y,6}, {0,y,6}}
    };
    const float scaled = (progress - std::floor(progress)) * 4.0f;
    const int segment = glm::min(static_cast<int>(scaled), 3);
    return bezier(p[segment][0], p[segment][1], p[segment][2], p[segment][3], scaled - segment);
}

void updateFreeCameraPosition()
{
    const float cp = std::cos(freePitch);
    cameraPosition = kFocus + glm::vec3(std::sin(freeYaw)*cp, std::sin(freePitch), std::cos(freeYaw)*cp) * freeRadius;
}

void keyCallback(GLFWwindow* window, int key, int, int action, int)
{
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) {
        mouseCaptured = !mouseCaptured;
        firstMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    } else if (key == GLFW_KEY_C) {
        cameraOnBezier = false;
        const glm::vec3 offset = cameraPosition - kFocus;
        freeRadius = glm::clamp(glm::length(offset), 2.5f, 30.0f);
        freeYaw = std::atan2(offset.x, offset.z);
        freePitch = std::asin(glm::clamp(offset.y / freeRadius, -1.0f, 1.0f));
    } else if (key == GLFW_KEY_V) {
        cameraOnBezier = true;
    }
}

void cursorPositionCallback(GLFWwindow*, double xpos, double ypos)
{
    if (!mouseCaptured) return;
    if (firstMouse) {
        lastX = static_cast<float>(xpos); lastY = static_cast<float>(ypos); firstMouse = false;
        return;
    }
    const float dx = static_cast<float>(xpos) - lastX;
    const float dy = lastY - static_cast<float>(ypos);
    lastX = static_cast<float>(xpos); lastY = static_cast<float>(ypos);
    if (!cameraOnBezier) {
        freeYaw += dx * 0.003f;
        freePitch = glm::clamp(freePitch + dy * 0.003f, -1.2f, 1.2f);
        updateFreeCameraPosition();
    }
}

GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024]{}; glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Error al compilar shader: " << log << '\n';
        glDeleteShader(shader); return 0;
    }
    return shader;
}

GLuint createProgram()
{
    const char* vertexSource = R"glsl(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec4 aColor;
layout(location=2) in vec2 aTexCoord;
layout(location=3) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat3 uNormalMatrix;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec4 vColor;
out vec2 vTexCoord;
void main() {
    vec4 world = uModel * vec4(aPosition, 1.0);
    vWorldPosition = world.xyz;
    vWorldNormal = normalize(uNormalMatrix * aNormal);
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = uMVP * vec4(aPosition, 1.0);
})glsl";
    const char* fragmentSource = R"glsl(#version 330 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec4 vColor;
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform sampler2D uEmissiveTexture;
uniform vec4 uBaseColor;
uniform vec3 uEmissive;
uniform vec3 uCameraPosition;
uniform int uHasTexture;
uniform int uHasEmissiveTexture;
uniform int uAlphaMask;
uniform float uAlphaCutoff;
void main() {
    vec4 base = (uHasTexture != 0 ? texture(uTexture, vTexCoord) : vec4(1.0)) * vColor * uBaseColor;
    if (uAlphaMask != 0 && base.a < uAlphaCutoff) discard;
    // Phong: la luz puntual sigue la posicion de la camara en cada fotograma.
    vec3 normal = normalize(vWorldNormal);
    vec3 lightDir = normalize(uCameraPosition - vWorldPosition);
    vec3 viewDir = normalize(uCameraPosition - vWorldPosition);
    float diffuse = max(dot(normal, lightDir), 0.0);
    float specular = pow(max(dot(viewDir, reflect(-lightDir, normal)), 0.0), 32.0);
    float d = length(uCameraPosition - vWorldPosition);
    float attenuation = 1.0 / (1.0 + 0.025*d + 0.006*d*d);
    vec3 phong = 0.22*base.rgb + attenuation*(0.80*diffuse*base.rgb + 0.30*specular);
    vec3 emissive = uEmissive * (uHasEmissiveTexture != 0 ? texture(uEmissiveTexture, vTexCoord).rgb : vec3(1.0));
    FragColor = vec4(phong + emissive, base.a);
})glsl";
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex); if (fragment) glDeleteShader(fragment); return 0;
    }
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex); glAttachShader(program, fragment); glLinkProgram(program);
    glDeleteShader(vertex); glDeleteShader(fragment);
    GLint ok = GL_FALSE; glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024]{}; glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Error al enlazar shaders: " << log << '\n'; glDeleteProgram(program); return 0;
    }
    return program;
}
} // namespace

int main()
{
    if (!glfwInit()) { std::cerr << "No se pudo inicializar GLFW.\n"; return -1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1000, 800, "YoutubeOpenGL - Edificio Atlanta", nullptr, nullptr);
    if (!window) { std::cerr << "No se pudo crear la ventana OpenGL.\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "No se pudieron cargar las funciones de OpenGL.\n";
        glfwDestroyWindow(window); glfwTerminate(); return -1;
    }
    glEnable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, cursorPositionCallback);
    glfwSetKeyCallback(window, keyCallback);
    GLuint program = createProgram();
    if (!program) { glfwDestroyWindow(window); glfwTerminate(); return -1; }

    GltfModel building;
    if (!building.load("resources/atlanta_office/scene.gltf")) {
        std::cerr << building.error() << '\n'; glDeleteProgram(program);
        glfwDestroyWindow(window); glfwTerminate(); return -1;
    }
    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true); ImGui_ImplOpenGL3_Init("#version 330");

    float previousTime = static_cast<float>(glfwGetTime());
    int projectionWidth = -1, projectionHeight = -1;
    glm::mat4 projection(1.0f);
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const float now = static_cast<float>(glfwGetTime());
        const float dt = glm::min(now - previousTime, 0.05f); previousTime = now;
        if (cameraOnBezier) {
            pathProgress = std::fmod(pathProgress + dt/25.0f, 1.0f);
            cameraPosition = bezierCameraPosition(pathProgress);
        } else {
            const float move = 3.0f * dt;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) freeRadius -= move;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) freeRadius += move;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) freeYaw -= move/freeRadius;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) freeYaw += move/freeRadius;
            freeRadius = glm::clamp(freeRadius, 2.5f, 30.0f);
            updateFreeCameraPosition();
        }

        int width = 0, height = 0; glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0) { glfwWaitEventsTimeout(0.05); continue; }
        if (width != projectionWidth || height != projectionHeight) {
            projectionWidth = width; projectionHeight = height;
            projection = glm::perspective(glm::radians(60.0f),
                static_cast<float>(width)/static_cast<float>(height), 0.1f, 100.0f);
        }
        glViewport(0, 0, width, height);
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        const glm::mat4 view = glm::lookAt(cameraPosition, kFocus, kUp);
        glUseProgram(program);
        building.draw(program, view, projection, cameraPosition);

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGui::Begin("Camara y controles");
        ImGui::Text("Modelo glTF: edificio de oficinas de Atlanta");
        ImGui::Text("Modo: %s", cameraOnBezier ? "recorrido Bezier" : "orbita libre enfocada al modelo");
        ImGui::Text("W/S: acercar o alejar | A/D: rodear el modelo");
        ImGui::Text("Mouse: orbitar libre | C: salir de curva | V: volver a curva");
        ImGui::Text("Esc: liberar/capturar cursor"); ImGui::End();
        ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown(); ImGui::DestroyContext();
    building.destroy(); glDeleteProgram(program); glfwDestroyWindow(window); glfwTerminate();
    return 0;
}
