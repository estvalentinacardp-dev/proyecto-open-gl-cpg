#include <iostream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
// librería para cargar modelos glTF
#include "GltfModel.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// librería para cargar imágenes
static glm::vec3 camPos(0.0f, 0.0f, 3.5f);
static glm::vec3 camFront(0.0f, 0.0f, -1.0f);
static glm::vec3 camUp(0.0f, 1.0f, 0.0f);
static float yaw = -90.0f;
static float pitch = 0.0f;
static float lastX = 400.0f;
static float lastY = 400.0f;
static bool firstMouse = true;
static bool mouseCaptured = true;
// Callback para manejar la captura del mouse
static void key_callback(GLFWwindow* window, int key, int, int action, int)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        mouseCaptured = !mouseCaptured;
        firstMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR,
            mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
}
// Callback para manejar el movimiento del mouse
static void cursor_position_callback(GLFWwindow*, double xpos, double ypos)
{
    if (!mouseCaptured) return;
    if (firstMouse) {
        lastX = static_cast<float>(xpos);
        lastY = static_cast<float>(ypos);
        firstMouse = false;
    }

    constexpr float sensitivity = 0.1f;
    yaw += (static_cast<float>(xpos) - lastX) * sensitivity;
    pitch += (lastY - static_cast<float>(ypos)) * sensitivity;
    lastX = static_cast<float>(xpos);
    lastY = static_cast<float>(ypos);
    pitch = glm::clamp(pitch, -89.0f, 89.0f);

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camFront = glm::normalize(front);
}
// Función para compilar un shader y verificar errores
static GLuint compile_shader(GLenum type, const char* source)
{
    const GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error: " << log << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_program()
{
    const char* vertexSource = R"glsl(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 aTexCoord;
out vec4 vertexColor;
out vec2 texCoord;
uniform mat4 uMVP;
void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vertexColor = aColor;
    texCoord = aTexCoord;
})glsl";

    const char* fragmentSource = R"glsl(#version 330 core
in vec4 vertexColor;
in vec2 texCoord;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform sampler2D uEmissiveTexture;
uniform vec4 uBaseColor;
uniform vec3 uEmissive;
uniform int uHasTexture;
uniform int uHasEmissiveTexture;
uniform int uAlphaMask;
uniform float uAlphaCutoff;
void main() {
    vec4 base = (uHasTexture != 0 ? texture(uTexture, texCoord) : vec4(1.0)) * vertexColor * uBaseColor;
    if (uAlphaMask != 0 && base.a < uAlphaCutoff) discard;
    vec3 emissive = uEmissive * (uHasEmissiveTexture != 0 ? texture(uEmissiveTexture, texCoord).rgb : vec3(1.0));
    FragColor = vec4(base.rgb + emissive, base.a);
})glsl";

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertex || !fragment) {
        if (vertex) glDeleteShader(vertex);
        if (fragment) glDeleteShader(fragment);
        return 0;
    }

    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE) {
        char log[1024]{};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::cerr << "Shader program link error: " << log << std::endl;
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

int main()
{
    if (!glfwInit()) {
        std::cerr << "No se pudo inicializar GLFW." << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1000, 800, "YoutubeOpenGL - Edificio Atlanta", nullptr, nullptr);
    if (!window) {
        std::cerr << "No se pudo crear la ventana OpenGL." << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "No se pudieron cargar las funciones de OpenGL." << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glfwSwapInterval(1);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetKeyCallback(window, key_callback);

    const GLuint program = create_program();
    if (!program) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    GltfModel building;
    const std::string modelPath = "resources/atlanta_office/scene.gltf";
    if (!building.load(modelPath)) {
        std::cerr << building.error() << std::endl;
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    float previousTime = static_cast<float>(glfwGetTime());
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        const float now = static_cast<float>(glfwGetTime());
        const float deltaTime = glm::min(now - previousTime, 0.05f);
        previousTime = now;

        if (mouseCaptured) {
            const glm::vec3 right = glm::normalize(glm::cross(camFront, camUp));
            glm::vec3 movement(0.0f);
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) movement += camFront;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) movement -= camFront;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) movement -= right;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) movement += right;
            if (glm::dot(movement, movement) > 0.0f)
                camPos += glm::normalize(movement) * (2.5f * deltaTime);
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        height = glm::max(height, 1);
        glViewport(0, 0, width, height);
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const glm::mat4 projection = glm::perspective(
            glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(camPos, camPos + camFront, camUp);
        glUseProgram(program);
        building.draw(program, projection * view);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Controles");
        ImGui::Text("Modelo glTF: edificio de oficinas de Atlanta");
        ImGui::Text("WASD: mover camara");
        ImGui::Text("Mouse: mirar | Escape: liberar/capturar cursor");
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    building.destroy();
    glDeleteProgram(program);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}