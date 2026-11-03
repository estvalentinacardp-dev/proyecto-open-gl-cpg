#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "GltfModel.h"
#ifdef APIENTRY
#undef APIENTRY
#endif
#include "tiny_gltf.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
// Función auxiliar para obtener un puntero a los datos de un accesor y su stride
namespace {
const unsigned char* accessorData(const tinygltf::Model& model, const tinygltf::Accessor& accessor, size_t& stride)
{
    if (accessor.bufferView < 0 || static_cast<size_t>(accessor.bufferView) >= model.bufferViews.size()) return nullptr;
    const auto& view = model.bufferViews[accessor.bufferView];
    if (view.buffer < 0 || static_cast<size_t>(view.buffer) >= model.buffers.size()) return nullptr;
    stride = static_cast<size_t>(accessor.ByteStride(view));
    if (stride == 0 || stride == static_cast<size_t>(-1)) return nullptr;
    const auto& bytes = model.buffers[view.buffer].data;
    const size_t offset = view.byteOffset + accessor.byteOffset;
    if (offset > bytes.size()) return nullptr;
    return bytes.data() + offset;
}
// Función auxiliar para obtener la transformación de un nodo
glm::mat4 nodeTransform(const tinygltf::Node& node)
{
    if (node.matrix.size() == 16) {
        glm::mat4 matrix(1.0f);
        for (size_t i = 0; i < 16; ++i)
            matrix[static_cast<int>(i / 4)][static_cast<int>(i % 4)] = static_cast<float>(node.matrix[i]);
        return matrix;
    }

    glm::mat4 transform(1.0f);
    if (node.translation.size() == 3)
        transform = glm::translate(transform, glm::vec3(node.translation[0], node.translation[1], node.translation[2]));
    if (node.rotation.size() == 4) {
        glm::quat rotation(static_cast<float>(node.rotation[3]), static_cast<float>(node.rotation[0]),
                           static_cast<float>(node.rotation[1]), static_cast<float>(node.rotation[2]));
        transform *= glm::mat4_cast(rotation);
    }
    if (node.scale.size() == 3)
        transform = glm::scale(transform, glm::vec3(node.scale[0], node.scale[1], node.scale[2]));
    return transform;
}

void collectNodeBounds(const tinygltf::Model& model, int nodeIndex, const glm::mat4& parent,
                       glm::vec3& minimum, glm::vec3& maximum, bool& hasBounds)
{
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model.nodes.size()) return;
    const auto& node = model.nodes[nodeIndex];
    const glm::mat4 world = parent * nodeTransform(node);
    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < model.meshes.size()) {
        for (const auto& primitive : model.meshes[node.mesh].primitives) {
            const auto position = primitive.attributes.find("POSITION");
            if (position == primitive.attributes.end() || position->second < 0 ||
                static_cast<size_t>(position->second) >= model.accessors.size()) continue;
            const auto& accessor = model.accessors[position->second];
            if (accessor.minValues.size() != 3 || accessor.maxValues.size() != 3) continue;
            for (int corner = 0; corner < 8; ++corner) {
                const glm::vec3 point(
                    static_cast<float>((corner & 1) ? accessor.maxValues[0] : accessor.minValues[0]),
                    static_cast<float>((corner & 2) ? accessor.maxValues[1] : accessor.minValues[1]),
                    static_cast<float>((corner & 4) ? accessor.maxValues[2] : accessor.minValues[2]));
                const glm::vec3 transformed(world * glm::vec4(point, 1.0f));
                minimum = glm::min(minimum, transformed);
                maximum = glm::max(maximum, transformed);
                hasBounds = true;
            }
        }
    }
    for (int child : node.children) collectNodeBounds(model, child, world, minimum, maximum, hasBounds);
}

GLuint textureFor(const tinygltf::Model& model, const std::vector<GLuint>& textures, int textureIndex)
{
    if (textureIndex < 0 || static_cast<size_t>(textureIndex) >= model.textures.size()) return 0;
    const int image = model.textures[textureIndex].source;
    if (image < 0 || static_cast<size_t>(image) >= textures.size()) return 0;
    return textures[image];
}
}

bool GltfModel::load(const std::string& path)
{
    destroy();
    auto parsedModel = std::make_unique<tinygltf::Model>();
    tinygltf::TinyGLTF loader;
    std::string warning;
    stbi_set_flip_vertically_on_load(1);
    if (!loader.LoadASCIIFromFile(parsedModel.get(), &error_, &warning, path)) {
        error_ = "No se pudo cargar glTF: " + error_;
        return false;
    }
    if (!warning.empty()) std::cerr << "glTF: " << warning << '\n';
    model_ = parsedModel.release();

    imageTextures_.resize(model_->images.size(), 0);
    std::vector<bool> usedImages(model_->images.size(), false);
    for (const auto& material : model_->materials) {
        const int textureIndices[] = { material.pbrMetallicRoughness.baseColorTexture.index, material.emissiveTexture.index };
        for (int textureIndex : textureIndices) {
            if (textureIndex >= 0 && static_cast<size_t>(textureIndex) < model_->textures.size()) {
                const int imageIndex = model_->textures[textureIndex].source;
                if (imageIndex >= 0 && static_cast<size_t>(imageIndex) < usedImages.size()) usedImages[imageIndex] = true;
            }
        }
    }
    for (size_t i = 0; i < model_->images.size(); ++i) {
        if (!usedImages[i]) continue;
        const auto& image = model_->images[i];
        if (image.image.empty() || image.width <= 0 || image.height <= 0) continue;
        GLenum format = GL_RGBA, internalFormat = GL_SRGB8_ALPHA8;
        if (image.component == 1) { format = GL_RED; internalFormat = GL_R8; }
        else if (image.component == 2) { format = GL_RG; internalFormat = GL_RG8; }
        else if (image.component == 3) { format = GL_RGB; internalFormat = GL_SRGB8; }
        else if (image.component != 4) continue;
        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, image.width, image.height, 0,
                     format, GL_UNSIGNED_BYTE, image.image.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        imageTextures_[i] = texture;
    }
    for (auto& image : model_->images) {
        image.image.clear();
        image.image.shrink_to_fit();
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    materials_.resize(model_->materials.size());
    for (size_t i = 0; i < model_->materials.size(); ++i) {
        const auto& source = model_->materials[i];
        auto& target = materials_[i];
        const auto& pbr = source.pbrMetallicRoughness;
        if (pbr.baseColorFactor.size() == 4)
            target.baseColor = glm::vec4(pbr.baseColorFactor[0], pbr.baseColorFactor[1], pbr.baseColorFactor[2], pbr.baseColorFactor[3]);
        if (source.emissiveFactor.size() == 3)
            target.emissive = glm::vec3(source.emissiveFactor[0], source.emissiveFactor[1], source.emissiveFactor[2]);
        target.baseColorTexture = textureFor(*model_, imageTextures_, pbr.baseColorTexture.index);
        target.emissiveTexture = textureFor(*model_, imageTextures_, source.emissiveTexture.index);
        target.alphaBlend = source.alphaMode == "BLEND";
        target.alphaMask = source.alphaMode == "MASK";
        target.alphaCutoff = static_cast<float>(source.alphaCutoff);
    }

    meshes_.resize(model_->meshes.size());
    for (size_t meshIndex = 0; meshIndex < model_->meshes.size(); ++meshIndex) {
        const auto& mesh = model_->meshes[meshIndex];
        for (const auto& primitive : mesh.primitives) {
            if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) continue;
            const auto positionAttribute = primitive.attributes.find("POSITION");
            if (positionAttribute == primitive.attributes.end() || primitive.indices < 0) continue;
            const auto& positions = model_->accessors[positionAttribute->second];
            if (positions.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || positions.type != TINYGLTF_TYPE_VEC3) continue;
            size_t positionStride = 0;
            const unsigned char* positionBytes = accessorData(*model_, positions, positionStride);
            if (!positionBytes) continue;

            const auto uvIt = primitive.attributes.find("TEXCOORD_0");
            const tinygltf::Accessor* uvs = uvIt == primitive.attributes.end() ? nullptr : &model_->accessors[uvIt->second];
            size_t uvStride = 0;
            const unsigned char* uvBytes = uvs ? accessorData(*model_, *uvs, uvStride) : nullptr;
            const auto colorIt = primitive.attributes.find("COLOR_0");
            const tinygltf::Accessor* colors = colorIt == primitive.attributes.end() ? nullptr : &model_->accessors[colorIt->second];
            size_t colorStride = 0;
            const unsigned char* colorBytes = colors ? accessorData(*model_, *colors, colorStride) : nullptr;

            std::vector<float> vertices(positions.count * 9, 0.0f);
            for (size_t i = 0; i < positions.count; ++i) {
                const auto* p = reinterpret_cast<const float*>(positionBytes + i * positionStride);
                float* v = vertices.data() + i * 9;
                v[0] = p[0]; v[1] = p[1]; v[2] = p[2];
                v[3] = v[4] = v[5] = v[6] = 1.0f;
                if (uvs && uvBytes && uvs->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && uvs->type == TINYGLTF_TYPE_VEC2) {
                    const auto* uv = reinterpret_cast<const float*>(uvBytes + i * uvStride);
                    v[7] = uv[0]; v[8] = uv[1];
                }
                if (colors && colorBytes && colors->componentType == TINYGLTF_COMPONENT_TYPE_FLOAT &&
                    (colors->type == TINYGLTF_TYPE_VEC3 || colors->type == TINYGLTF_TYPE_VEC4)) {
                    const auto* c = reinterpret_cast<const float*>(colorBytes + i * colorStride);
                    v[3] = c[0]; v[4] = c[1]; v[5] = c[2];
                    if (colors->type == TINYGLTF_TYPE_VEC4) v[6] = c[3];
                }
            }

            const auto& indices = model_->accessors[primitive.indices];
            size_t indexStride = 0;
            const unsigned char* indexBytes = accessorData(*model_, indices, indexStride);
            if (!indexBytes || indices.type != TINYGLTF_TYPE_SCALAR) continue;
            GLenum indexType = GL_UNSIGNED_INT;
            std::vector<uint32_t> convertedIndices;
            const void* indexData = indexBytes;
            size_t indexSize = indices.count;
            if (indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
                convertedIndices.reserve(indices.count);
                for (size_t i = 0; i < indices.count; ++i) {
                    uint32_t value = 0;
                    if (indices.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        value = *reinterpret_cast<const uint16_t*>(indexBytes + i * indexStride);
                    else value = *(indexBytes + i * indexStride);
                    convertedIndices.push_back(value);
                }
                indexData = convertedIndices.data();
                indexType = GL_UNSIGNED_INT;
            } else if (indices.componentType != TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) continue;

            PrimitiveGpu gpu;
            gpu.indexCount = static_cast<GLsizei>(indexSize);
            gpu.indexType = indexType;
            gpu.material = primitive.material;
            glGenVertexArrays(1, &gpu.vao);
            glGenBuffers(1, &gpu.vbo);
            glGenBuffers(1, &gpu.ebo);
            glBindVertexArray(gpu.vao);
            glBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexSize * sizeof(uint32_t)), indexData, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), nullptr);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 9 * sizeof(float), reinterpret_cast<void*>(7 * sizeof(float)));
            glEnableVertexAttribArray(2);
            meshes_[meshIndex].primitives.push_back(gpu);
        }
    }
    glBindVertexArray(0);

    glm::vec3 minBounds(std::numeric_limits<float>::max());
    glm::vec3 maxBounds(std::numeric_limits<float>::lowest());
    bool hasBounds = false;
    if (model_->defaultScene >= 0 && static_cast<size_t>(model_->defaultScene) < model_->scenes.size()) {
        for (int node : model_->scenes[model_->defaultScene].nodes)
            collectNodeBounds(*model_, node, glm::mat4(1.0f), minBounds, maxBounds, hasBounds);
    } else if (!model_->scenes.empty()) {
        for (int node : model_->scenes[0].nodes)
            collectNodeBounds(*model_, node, glm::mat4(1.0f), minBounds, maxBounds, hasBounds);
    }
    if (hasBounds) {
        const glm::vec3 extent = maxBounds - minBounds;
        const float largest = std::max(extent.x, std::max(extent.y, extent.z));
        const float scale = largest > 0.0f ? 3.0f / largest : 1.0f;
        normalization_ = glm::scale(glm::mat4(1.0f), glm::vec3(scale)) *
            glm::translate(glm::mat4(1.0f), -(minBounds + maxBounds) * 0.5f);
    }

    loaded_ = true;
    std::cout << "Modelo glTF cargado: " << model_->meshes.size() << " mallas, "
              << model_->materials.size() << " materiales, " << model_->images.size() << " texturas\n";
    return true;
}

void GltfModel::draw(GLuint program, const glm::mat4& viewProjection)
{
    if (!loaded_ || !model_) return;
    if (!uniformsCached_) {
        mvpLocation_ = glGetUniformLocation(program, "uMVP");
        colorLocation_ = glGetUniformLocation(program, "uBaseColor");
        emissiveLocation_ = glGetUniformLocation(program, "uEmissive");
        hasTextureLocation_ = glGetUniformLocation(program, "uHasTexture");
        hasEmissiveLocation_ = glGetUniformLocation(program, "uHasEmissiveTexture");
        alphaMaskLocation_ = glGetUniformLocation(program, "uAlphaMask");
        alphaCutoffLocation_ = glGetUniformLocation(program, "uAlphaCutoff");
        glUniform1i(glGetUniformLocation(program, "uTexture"), 0);
        glUniform1i(glGetUniformLocation(program, "uEmissiveTexture"), 1);
        uniformsCached_ = true;
    }

    glm::mat4 root = viewProjection * normalization_;
    if (model_->defaultScene >= 0 && static_cast<size_t>(model_->defaultScene) < model_->scenes.size()) {
        for (int node : model_->scenes[model_->defaultScene].nodes) drawNode(node, root, program);
    } else if (!model_->scenes.empty()) {
        for (int node : model_->scenes[0].nodes) drawNode(node, root, program);
    } else {
        for (size_t i = 0; i < model_->nodes.size(); ++i)
            if (model_->nodes[i].mesh >= 0) drawNode(static_cast<int>(i), root, program);
    }
}

void GltfModel::drawNode(int nodeIndex, const glm::mat4& parent, GLuint program)
{
    if (nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= model_->nodes.size()) return;
    const auto& node = model_->nodes[nodeIndex];
    const glm::mat4 transform = parent * nodeTransform(node);
    if (node.mesh >= 0 && static_cast<size_t>(node.mesh) < meshes_.size()) {
        for (const auto& primitive : meshes_[node.mesh].primitives) {
            MaterialGpu material;
            if (primitive.material >= 0 && static_cast<size_t>(primitive.material) < materials_.size())
                material = materials_[primitive.material];
            glUniformMatrix4fv(mvpLocation_, 1, GL_FALSE, glm::value_ptr(transform));
            glUniform4fv(colorLocation_, 1, glm::value_ptr(material.baseColor));
            glUniform3fv(emissiveLocation_, 1, glm::value_ptr(material.emissive));
            glUniform1i(hasTextureLocation_, material.baseColorTexture != 0);
            glUniform1i(hasEmissiveLocation_, material.emissiveTexture != 0);
            glUniform1i(alphaMaskLocation_, material.alphaMask);
            glUniform1f(alphaCutoffLocation_, material.alphaCutoff);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, material.baseColorTexture);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, material.emissiveTexture);
            if (material.alphaBlend) {
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            } else {
                glDisable(GL_BLEND);
                glDepthMask(GL_TRUE);
            }
            glBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES, primitive.indexCount, primitive.indexType, nullptr);
            glBindVertexArray(0);
        }
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    for (int child : node.children) drawNode(child, transform, program);
}

void GltfModel::destroy()
{
    for (auto& mesh : meshes_) for (auto& primitive : mesh.primitives) {
        if (primitive.ebo) glDeleteBuffers(1, &primitive.ebo);
        if (primitive.vbo) glDeleteBuffers(1, &primitive.vbo);
        if (primitive.vao) glDeleteVertexArrays(1, &primitive.vao);
    }
    for (GLuint texture : imageTextures_) if (texture) glDeleteTextures(1, &texture);
    meshes_.clear(); imageTextures_.clear(); materials_.clear();
    delete model_; model_ = nullptr;
    loaded_ = false;
    uniformsCached_ = false;
}
