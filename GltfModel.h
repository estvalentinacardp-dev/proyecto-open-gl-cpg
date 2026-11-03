#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace tinygltf { class Model; }

class GltfModel {
public:
    bool load(const std::string& path);
    void draw(GLuint program, const glm::mat4& viewProjection);
    void destroy();
    const std::string& error() const { return error_; }
    bool isLoaded() const { return loaded_; }

private:
    struct PrimitiveGpu {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
        GLenum indexType = GL_UNSIGNED_INT;
        int material = -1;
    };
    struct MeshGpu { std::vector<PrimitiveGpu> primitives; };
    struct MaterialGpu {
        GLuint baseColorTexture = 0;
        GLuint emissiveTexture = 0;
        glm::vec4 baseColor = glm::vec4(1.0f);
        glm::vec3 emissive = glm::vec3(0.0f);
        bool alphaBlend = false;
        bool alphaMask = false;
        float alphaCutoff = 0.5f;
    };

    tinygltf::Model* model_ = nullptr;
    std::vector<MeshGpu> meshes_;
    std::vector<GLuint> imageTextures_;
    std::vector<MaterialGpu> materials_;
    glm::mat4 normalization_ = glm::mat4(1.0f);
    std::string error_;
    bool loaded_ = false;
    bool uniformsCached_ = false;
    GLint mvpLocation_ = -1;
    GLint colorLocation_ = -1;
    GLint emissiveLocation_ = -1;
    GLint hasTextureLocation_ = -1;
    GLint hasEmissiveLocation_ = -1;
    GLint alphaMaskLocation_ = -1;
    GLint alphaCutoffLocation_ = -1;

    void drawNode(int nodeIndex, const glm::mat4& parent, GLuint program);
};
