#ifdef __clang__
#pragma clang diagnostic ignored "-Weverything"
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
// then
#undef TINYGLTF_IMPLEMENTATION
#undef STB_IMAGE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#include "animation.hh"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "gltf-loader.hh"
#include "tiny_gltf_util.h"

void load_animations(const tinygltf::Model& model,
                     std::vector<animation>& animations) {
  const auto nb_animations = animations.size();
  for (int i = 0; i < nb_animations; ++i) {
    const auto& gltf_animation = model.animations[i];

    // Attempt to get an animation name, or generate one like "animation_x"
    animations[i].name = !gltf_animation.name.empty()
                             ? gltf_animation.name
                             : "animation_" + std::to_string(i);

    // Load samplers:
    animations[i].samplers.resize(gltf_animation.samplers.size());

    for (int sampler_index = 0; sampler_index < animations[i].samplers.size();
         ++sampler_index) {
      animations[i].samplers[sampler_index].mode = [&] {
        if (gltf_animation.samplers[sampler_index].interpolation == "LINEAR")
          return animation::sampler::interpolation::linear;
        if (gltf_animation.samplers[sampler_index].interpolation == "STEP")
          return animation::sampler::interpolation::step;
        if (gltf_animation.samplers[sampler_index].interpolation ==
            "CUBICSPLINE")
          return animation::sampler::interpolation::cubic_spline;
        return animation::sampler::interpolation::not_assigned;
      }();

      float min_v, max_v;
      tinygltf::util::GetAnimationSamplerInputMinMax(
          gltf_animation.samplers[sampler_index], model, &min_v, &max_v);
      animations[i].samplers[sampler_index].min_v = min_v;
      animations[i].samplers[sampler_index].max_v = max_v;

      const auto nb_frames = tinygltf::util::GetAnimationSamplerInputCount(
          gltf_animation.samplers[sampler_index], model);
      animations[i].samplers[sampler_index].keyframes.resize(nb_frames);

      float value = 0;
      for (int keyframe = 0; keyframe < nb_frames; ++keyframe) {
        tinygltf::util::DecodeScalarAnimationValue(
            size_t(keyframe),
            model.accessors[gltf_animation.samplers[sampler_index].input],
            model, &value);
        animations[i].samplers[sampler_index].keyframes[keyframe] =
            std::make_pair(keyframe, value);
      }
    }

    // Load channels
    animations[i].channels.resize(gltf_animation.channels.size());
    for (int channel_index = 0; channel_index < animations[i].channels.size();
         ++channel_index) {
      animations[i].channels[channel_index].target_node =
          gltf_animation.channels[channel_index].target_node;

      animations[i].channels[channel_index].sampler_index =
          gltf_animation.channels[channel_index].sampler;

      const auto& sampler =
          gltf_animation
              .samplers[gltf_animation.channels[channel_index].sampler];

      const auto nb_frames =
          tinygltf::util::GetAnimationSamplerOutputCount(sampler, model);
      animations[i].channels[channel_index].keyframes.resize(nb_frames);
      const auto& accessor = model.accessors[sampler.output];

      if (gltf_animation.channels[channel_index].target_path == "weights") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::weight;

        float value;

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeScalarAnimationValue(size_t(frame), accessor,
                                                     model, &value);
          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.weight = value;
        }
      }

      if (gltf_animation.channels[channel_index].target_path == "translation") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::translation;

        float xyz[3];

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeTranslationAnimationValue(size_t(frame),
                                                          accessor, model, xyz);
          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.translation = glm::make_vec3(xyz);
        }
      }

      if (gltf_animation.channels[channel_index].target_path == "rotation") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::rotation;

        float xyzw[4];

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeRotationAnimationValue(size_t(frame), accessor,
                                                       model, xyzw);
          glm::quat q;
          q.w = xyzw[3];
          q.x = xyzw[0];
          q.y = xyzw[1];
          q.z = xyzw[2];
          q = glm::normalize(q);

          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.rotation = q;
        }
      }

      if (gltf_animation.channels[channel_index].target_path == "scale") {
        animations[i].channels[channel_index].mode =
            animation::channel::path::scale;
        float xyz[3];

        for (int frame = 0; frame < nb_frames; ++frame) {
          tinygltf::util::DecodeScaleAnimationValue(size_t(frame), accessor,
                                                    model, xyz);
          animations[i].channels[channel_index].keyframes[frame].first = frame;
          animations[i]
              .channels[channel_index]
              .keyframes[frame]
              .second.motion.scale = glm::make_vec3(xyz);
        }
      }
    }

    animations[i].compute_time_boundaries();
  }
}

glm::vec3 generate_flat_normal_for_triangle(std::vector<float>& position,
                                            const unsigned i0,
                                            const unsigned i1,
                                            const unsigned i2) {
  const glm::vec3 v0(position[i0 + 0], position[i0 + 1], position[i0 + 2]);
  const glm::vec3 v1(position[i1 + 0], position[i1 + 1], position[i1 + 2]);
  const glm::vec3 v2(position[i2 + 0], position[i2 + 1], position[i2 + 2]);

  return glm::normalize(glm::cross(v0 - v1, v1 - v2));
}

void load_geometry(const tinygltf::Model& model, std::vector<GLuint>& textures,
                   const std::vector<tinygltf::Primitive>& primitives,
                   std::vector<draw_call_submesh>& draw_call_descriptor,
                   std::vector<GLuint>& VAOs,
                   std::vector<std::array<GLuint, 7>>& VBOs,
                   std::vector<std::vector<unsigned>>& indices,
                   std::vector<std::vector<float>>& vertex_coord,
                   std::vector<std::vector<float>>& texture_coord,
                   std::vector<std::vector<float>>& normals,
                   std::vector<std::vector<float>>& weights,
                   std::vector<std::vector<unsigned short>>& joints) {
  const auto nb_submeshes = primitives.size();

  for (size_t submesh = 0; submesh < nb_submeshes; ++submesh) {
    const auto& primitive = primitives[submesh];

    // We have one VAO per "submesh" (= gltf primitive)
    draw_call_descriptor[submesh].VAO = VAOs[submesh];
    // Primitive uses their own draw mode (eg: lines (for hairs?),
    // triangle fan/strip/list?)
    draw_call_descriptor[submesh].draw_mode = primitive.mode;

    // TODO refcator the accessor -> array loading

    // INDEX BUFFER
    {
      const auto& indices_accessor = model.accessors[primitive.indices];
      const auto& indices_buffer_view =
          model.bufferViews[indices_accessor.bufferView];
      const auto& indices_buffer = model.buffers[indices_buffer_view.buffer];
      const auto indices_start_pointer = indices_buffer.data.data() +
                                         indices_buffer_view.byteOffset +
                                         indices_accessor.byteOffset;
      const auto indices_stride =
          indices_accessor.ByteStride(indices_buffer_view);
      indices[submesh].resize(indices_accessor.count);
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(indices_accessor.componentType);
      assert(indices_accessor.type == TINYGLTF_TYPE_SCALAR);
      assert(sizeof(unsigned int) >= byte_size_of_component);

      for (size_t i = 0; i < indices_accessor.count; ++i) {
        unsigned int temp = 0;
        memcpy(&temp, indices_start_pointer + i * indices_stride,
               byte_size_of_component);
        indices[submesh][i] = unsigned(temp);
      }
      // number of elements to pass to glDrawElements(...)
      draw_call_descriptor[submesh].count = indices_accessor.count;
    }

    // VERTEX POSITIONS
    {
      const auto position = primitive.attributes.at("POSITION");
      const auto& position_accessor = model.accessors[position];
      const auto& position_buffer_view =
          model.bufferViews[position_accessor.bufferView];
      const auto& position_buffer = model.buffers[position_buffer_view.buffer];
      const auto position_stride =
          position_accessor.ByteStride(position_buffer_view);
      const auto position_start_pointer = position_buffer.data.data() +
                                          position_buffer_view.byteOffset +
                                          position_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(position_accessor.componentType);
      assert(position_accessor.type == TINYGLTF_TYPE_VEC3);
      assert(sizeof(double) >= byte_size_of_component);

      vertex_coord[submesh].resize(position_accessor.count * 3);
      for (size_t i = 0; i < position_accessor.count; ++i) {
        if (byte_size_of_component == sizeof(double)) {
          double temp[3];
          memcpy(&temp, position_start_pointer + i * position_stride,
                 byte_size_of_component * 3);
          for (size_t j = 0; j < 3; ++j) {
            vertex_coord[submesh][i * 3 + j] = float(temp[j]);
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&vertex_coord[submesh][i * 3],
                 position_start_pointer + i * position_stride,
                 byte_size_of_component * 3);
        }
      }
    }

    // VERTEX NORMAL
    bool generate_normals = false;
    if (primitive.attributes.find("NORMAL") != primitive.attributes.end()) {
      const auto normal = primitive.attributes.at("NORMAL");
      const auto& normal_accessor = model.accessors[normal];
      const auto& normal_buffer_view =
          model.bufferViews[normal_accessor.bufferView];
      const auto& normal_buffer = model.buffers[normal_buffer_view.buffer];
      const auto normal_stride = normal_accessor.ByteStride(normal_buffer_view);
      const auto normal_start_pointer = normal_buffer.data.data() +
                                        normal_buffer_view.byteOffset +
                                        normal_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(normal_accessor.componentType);
      assert(normal_accessor.type == TINYGLTF_TYPE_VEC3);
      assert(sizeof(double) >= byte_size_of_component);

      normals[submesh].resize(normal_accessor.count * 3);
      for (size_t i = 0; i < normal_accessor.count; ++i) {
        if (byte_size_of_component == sizeof(double)) {
          double temp[3];
          memcpy(&temp, normal_start_pointer + i * normal_stride,
                 byte_size_of_component * 3);
          for (size_t j = 0; j < 3; ++j) {
            normals[submesh][i * 3 + j] = float(temp[j]);  // downcast to
            // float
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&normals[submesh][i * 3],
                 normal_start_pointer + i * normal_stride,
                 byte_size_of_component * 3);
        }
      }
    } else {
      generate_normals = true;
    }

    // VERTEX UV
    if (textures.size() > 0) {
      const auto texture = primitive.attributes.at("TEXCOORD_0");
      const auto& texture_accessor = model.accessors[texture];
      const auto& texture_buffer_view =
          model.bufferViews[texture_accessor.bufferView];
      const auto& texture_buffer = model.buffers[texture_buffer_view.buffer];
      const auto texture_stride =
          texture_accessor.ByteStride(texture_buffer_view);
      const auto texture_start_pointer = texture_buffer.data.data() +
                                         texture_buffer_view.byteOffset +
                                         texture_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(texture_accessor.componentType);
      assert(texture_accessor.type == TINYGLTF_TYPE_VEC2);
      assert(sizeof(double) >= byte_size_of_component);

      texture_coord[submesh].resize(texture_accessor.count * 2);
      for (size_t i = 0; i < texture_accessor.count; ++i) {
        if (byte_size_of_component == sizeof(double)) {
          double temp[2];
          memcpy(&temp, texture_start_pointer + i * texture_stride,
                 byte_size_of_component * 2);
          for (size_t j = 0; j < 2; ++j) {
            texture_coord[submesh][i * 2 + j] = float(temp[j]);  // downcast
            // to float
          }
        } else if (byte_size_of_component == sizeof(float)) {
          memcpy(&texture_coord[submesh][i * 2],
                 texture_start_pointer + i * texture_stride,
                 byte_size_of_component * 2);
        }
      }
    }

    // VERTEX JOINTS ASSIGNMENT
    if (primitive.attributes.find("JOINTS_0") !=
        std::end(primitive.attributes)) {
      const auto joint = primitive.attributes.at("JOINTS_0");
      const auto& joints_accessor = model.accessors[joint];
      const auto& joints_buffer_view =
          model.bufferViews[joints_accessor.bufferView];
      const auto& joints_buffer = model.buffers[joints_buffer_view.buffer];
      const auto joints_stride = joints_accessor.ByteStride(joints_buffer_view);
      const auto joints_start_pointer = joints_buffer.data.data() +
                                        joints_buffer_view.byteOffset +
                                        joints_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(joints_accessor.componentType);
      assert(joints_accessor.type == TINYGLTF_TYPE_VEC4);
      assert(sizeof(unsigned short) >= byte_size_of_component);

      joints[submesh].resize(4 * joints_accessor.count);

      for (size_t i = 0; i < joints_accessor.count; ++i) {
        memcpy(&joints[submesh][i * 4],
               joints_start_pointer + i * joints_stride,
               byte_size_of_component * 4);
      }
    }

    // VERTEX BONE WEIGHTS
    if (primitive.attributes.find("WEIGHTS_0") !=
        std::end(primitive.attributes)) {
      const auto weight = primitive.attributes.at("WEIGHTS_0");
      const auto& weights_accessor = model.accessors[weight];
      const auto& weights_buffer_view =
          model.bufferViews[weights_accessor.bufferView];
      const auto& weights_buffer = model.buffers[weights_buffer_view.buffer];
      const auto weights_stride =
          weights_accessor.ByteStride(weights_buffer_view);
      const auto weights_start_pointer = weights_buffer.data.data() +
                                         weights_buffer_view.byteOffset +
                                         weights_accessor.byteOffset;
      const size_t byte_size_of_component =
          tinygltf::GetComponentSizeInBytes(weights_accessor.componentType);
      assert(weights_accessor.type == TINYGLTF_TYPE_VEC4);
      assert(sizeof(float) >= byte_size_of_component);

      weights[submesh].resize(4 * weights_accessor.count);

      for (size_t i = 0; i < weights_accessor.count; ++i) {
        if (weights_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
          memcpy(&weights[submesh][i * 4],
                 weights_start_pointer + i * weights_stride,
                 byte_size_of_component * 4);
        } else {
          // Must convert normalized unsigned value to floating point
          unsigned short temp = 0;
          for (int j = 0; j < 4; j++) {
            memcpy(&temp,
                   weights_start_pointer + i * weights_stride +
                       j * byte_size_of_component,
                   byte_size_of_component);
            weights[submesh][i * 4 + j] =
                float(temp) /
                (byte_size_of_component == 2 ? float(0xFFFF) : float(0xFF));
          }
        }
      }
    }

    if (generate_normals) {
      std::cerr << "Warn: Needed to generate flat normals for this model\n";
      // size of array should match
      normals[submesh].resize(vertex_coord[submesh].size());

      // TODO this assume primitve is TINYGLTF_MODE_TRIANGLES
      // for each triangle
      if (primitive.mode == TINYGLTF_MODE_TRIANGLES) {
        for (size_t tri = 0; tri < indices[submesh].size() / 3; ++tri) {
          const auto i0 = indices[submesh][3 * tri + 0];
          const auto i1 = indices[submesh][3 * tri + 1];
          const auto i2 = indices[submesh][3 * tri + 2];

          const glm::vec3 n = generate_flat_normal_for_triangle(
              vertex_coord[submesh], i0, i1, i2);

          normals[submesh][i0 + 0] = normals[submesh][i1 + 0] =
              normals[submesh][i2 + 0] = n.x;
          normals[submesh][i0 + 1] = normals[submesh][i1 + 1] =
              normals[submesh][i2 + 1] = n.y;
          normals[submesh][i0 + 2] = normals[submesh][i1 + 2] =
              normals[submesh][i2 + 2] = n.z;
        }
      } else {
        std::cerr << "Warn: a primitive of a mesh does not define "
                     "normals, and is not TRIANGLE primitive. The unlikely "
                     "scenario you were to lazy to implement happened.\n";
      }
    }

    {
      // GPU upload and shader layout association
      glBindVertexArray(VAOs[submesh]);

      // Layout "0" = vertex coordinates
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][0]);
      glBufferData(GL_ARRAY_BUFFER,
                   vertex_coord[submesh].size() * sizeof(float),
                   vertex_coord[submesh].data(), GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(0);

      // Layout "1" = vertex normal
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][1]);
      glBufferData(GL_ARRAY_BUFFER, normals[submesh].size() * sizeof(float),
                   normals[submesh].data(), GL_DYNAMIC_DRAW);
      glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(1);

      // We we haven't loaded any texture, don't even bother with UVs
      if (textures.size() > 0) {
        // Layout "2" = vertex UV
        glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][2]);
        glBufferData(GL_ARRAY_BUFFER,
                     texture_coord[submesh].size() * sizeof(float),
                     texture_coord[submesh].data(), GL_STATIC_DRAW);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                              nullptr);
        glEnableVertexAttribArray(2);
      }

      //// Tangent is layout 3
      // glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][3]);
      // glBufferData(GL_ARRAY_BUFFER, tangents[submesh].size() * sizeof(float),
      //             tangents[submesh].data(), GL_DYNAMIC_DRAW);
      // glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
      //                      nullptr);
      // glEnableVertexAttribArray(3);

      // Layout "3" joints assignment vector
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][4]);
      glBufferData(GL_ARRAY_BUFFER,
                   joints[submesh].size() * sizeof(unsigned short),
                   joints[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(4, 4, GL_UNSIGNED_SHORT, GL_FALSE,
                            4 * sizeof(unsigned short), nullptr);
      glEnableVertexAttribArray(4);

      // Layout "4" joints weights
      glBindBuffer(GL_ARRAY_BUFFER, VBOs[submesh][5]);
      glBufferData(GL_ARRAY_BUFFER, weights[submesh].size() * sizeof(float),
                   weights[submesh].data(), GL_STATIC_DRAW);
      glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                            nullptr);
      glEnableVertexAttribArray(5);

      // EBO
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, VBOs[submesh][6]);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   indices[submesh].size() * sizeof(unsigned),
                   indices[submesh].data(), GL_STATIC_DRAW);
      glBindVertexArray(0);
    }

    const auto& material = model.materials[primitive.material];
    if (textures.size() > 0)
      draw_call_descriptor[submesh].main_texture =
          textures[material.values.at("baseColorTexture").TextureIndex()];
  }
}

void load_morph_targets(const tinygltf::Model& model,
                        const tinygltf::Primitive& primitive,
                        std::vector<morph_target>& morph_targets,
                        bool& has_normal, bool& has_tangent) {
  for (size_t i = 0; i < morph_targets.size(); ++i) {
    const auto& target = primitive.targets[i];

    const auto position_it = target.find("POSITION");
    const auto normal_it = target.find("NORMAL");
    const auto tangent_it = target.find("TANGENT");

    if (position_it != target.end()) {
      const auto& position_accessor = model.accessors[position_it->second];
      const auto& position_buffer_view =
          model.bufferViews[position_accessor.bufferView];
      const auto& position_buffer = model.buffers[position_buffer_view.buffer];
      const auto position_data_start = position_buffer.data.data() +
                                       position_buffer_view.byteOffset +
                                       position_accessor.byteOffset;
      const auto stride = position_accessor.ByteStride(position_buffer_view);

      assert(position_accessor.type == TINYGLTF_TYPE_VEC3);
      assert(position_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

      morph_targets[i].position.resize(3 * position_accessor.count);
      for (size_t vertex = 0; vertex < position_accessor.count; ++vertex) {
        memcpy(&morph_targets[i].position[3 * vertex],
               position_data_start + vertex * stride, sizeof(float) * 3);
      }

      if (position_accessor.sparse.isSparse) {
        const auto sparse_indices = position_accessor.sparse.indices;
        const auto indices_component_size = tinygltf::GetComponentSizeInBytes(
            position_accessor.sparse.indices.componentType);
        const auto& indices_buffer_view =
            model.bufferViews[sparse_indices.bufferView];
        const auto& indices_buffer = model.buffers[indices_buffer_view.buffer];
        const auto indices_data = indices_buffer.data.data() +
                                  indices_buffer_view.byteOffset +
                                  sparse_indices.byteOffset;

        assert(sizeof(unsigned int) >= indices_component_size);

        const auto sparse_values = position_accessor.sparse.values;
        const auto& values_buffer_view =
            model.bufferViews[sparse_values.bufferView];
        const auto& values_buffer = model.buffers[values_buffer_view.buffer];
        const float* values_data = reinterpret_cast<const float*>(
            values_buffer.data.data() + values_buffer_view.byteOffset +
            sparse_values.byteOffset);

        std::vector<unsigned int> indices(position_accessor.sparse.count);
        std::vector<float> values(3 * size_t(position_accessor.sparse.count));

        for (size_t sparse_index = 0; sparse_index < indices.size();
             ++sparse_index) {
          memcpy(&indices[sparse_index],
                 indices_data + sparse_index * indices_component_size,
                 indices_component_size);
          memcpy(&values[3 * sparse_index], &values_data[3 * sparse_index],
                 3 * sizeof(float));
        }

        // Patch the loaded sparse data into the morph target vertex attribute
        for (size_t sparse_index = 0; sparse_index < indices.size();
             ++sparse_index) {
          memcpy(&morph_targets[i].position[indices[sparse_index]],
                 &values[sparse_index * 3], 3 * sizeof(float));
        }
      }
    }

    if (normal_it != target.end()) {
      has_normal = true;
      const auto& normal_accessor = model.accessors[normal_it->second];
      const auto& normal_buffer_view =
          model.bufferViews[normal_accessor.bufferView];
      const auto& normal_buffer = model.buffers[normal_buffer_view.buffer];
      const auto normal_data_start = normal_buffer.data.data() +
                                     normal_buffer_view.byteOffset +
                                     normal_accessor.byteOffset;
      const auto stride = normal_accessor.ByteStride(normal_buffer_view);

      assert(normal_accessor.type == TINYGLTF_TYPE_VEC3);
      assert(normal_accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

      morph_targets[i].normal.resize(3 * normal_accessor.count);
      for (size_t vertex = 0; vertex < normal_accessor.count; ++vertex) {
        memcpy(&morph_targets[i].normal[3 * vertex],
               normal_data_start + vertex * stride, sizeof(float) * 3);
      }

      if (normal_accessor.sparse.isSparse) {
        const auto sparse_indices = normal_accessor.sparse.indices;
        const auto indices_component_size = tinygltf::GetComponentSizeInBytes(
            normal_accessor.sparse.indices.componentType);
        const auto& indices_buffer_view =
            model.bufferViews[sparse_indices.bufferView];
        const auto& indices_buffer = model.buffers[indices_buffer_view.buffer];
        const auto indices_data = indices_buffer.data.data() +
                                  indices_buffer_view.byteOffset +
                                  sparse_indices.byteOffset;

        assert(sizeof(unsigned int) >= indices_component_size);

        const auto sparse_values = normal_accessor.sparse.values;
        const auto& values_buffer_view =
            model.bufferViews[sparse_values.bufferView];
        const auto& values_buffer = model.buffers[values_buffer_view.buffer];
        const float* values_data = reinterpret_cast<const float*>(
            values_buffer.data.data() + values_buffer_view.byteOffset +
            sparse_values.byteOffset);

        std::vector<unsigned int> indices(normal_accessor.sparse.count);
        std::vector<float> values(3 * size_t(normal_accessor.sparse.count));

        for (size_t sparse_index = 0; sparse_index < indices.size();
             ++sparse_index) {
          memcpy(&indices[sparse_index],
                 indices_data + sparse_index * indices_component_size,
                 indices_component_size);
          memcpy(&values[3 * sparse_index], &values_data[3 * sparse_index],
                 3 * sizeof(float));
        }

        // Patch the loaded sparse data into the morph target vertex attribute
        for (size_t sparse_index = 0; sparse_index < indices.size();
             ++sparse_index) {
          memcpy(&morph_targets[i].normal[indices[sparse_index]],
                 &values[sparse_index * 3], 3 * sizeof(float));
        }
      }
    }
  }
}

void load_morph_target_names(const tinygltf::Mesh& mesh,
                             std::vector<std::string>& names) {
  if (mesh.extras.IsObject() && mesh.extras.Has("targetNames")) {
    const auto& targetNames = mesh.extras.Get("targetNames");
    if (targetNames.IsArray()) {
      assert(names.size() == targetNames.ArrayLen());
      for (size_t i = 0; i < names.size(); ++i) {
        assert(targetNames.Get(int(i)).IsString());
        names[i] = targetNames.Get(int(i)).Get<std::string>();
      }
    }
  }
}

void load_inverse_bind_matrix_array(
    tinygltf::Model model, const tinygltf::Skin& skin, size_t nb_joints,
    std::vector<glm::mat4>& inverse_bind_matrices) {
  // Two :  we need to get the inverse bind matrix array, as it is
  // necessary for skinning
  const auto& inverse_bind_matrices_accessor =
      model.accessors[skin.inverseBindMatrices];
  assert(inverse_bind_matrices_accessor.type == TINYGLTF_TYPE_MAT4);
  assert(inverse_bind_matrices_accessor.count == nb_joints);

  const auto& inverse_bind_matrices_bufferview =
      model.bufferViews[inverse_bind_matrices_accessor.bufferView];
  const auto& inverse_bind_matrices_buffer =
      model.buffers[inverse_bind_matrices_bufferview.buffer];
  const size_t inverse_bind_matrices_stride =
      inverse_bind_matrices_accessor.ByteStride(
          inverse_bind_matrices_bufferview);
  const auto inverse_bind_matrices_data_start =
      inverse_bind_matrices_buffer.data.data() +
      inverse_bind_matrices_accessor.byteOffset +
      inverse_bind_matrices_bufferview.byteOffset;
  const size_t inverse_bind_matrices_component_size =
      tinygltf::GetComponentSizeInBytes(
          inverse_bind_matrices_accessor.componentType);
  assert(sizeof(double) >= inverse_bind_matrices_component_size);

  inverse_bind_matrices.resize(nb_joints);

  for (size_t i = 0; i < nb_joints; ++i) {
    if (inverse_bind_matrices_component_size == sizeof(float)) {
      float temp[16];
      memcpy(
          temp,
          inverse_bind_matrices_data_start + i * inverse_bind_matrices_stride,
          inverse_bind_matrices_component_size * 16);
      inverse_bind_matrices[i] = glm::make_mat4(temp);
    }
    // TODO actually, in the glTF spec there's no mention of  supports for
    // doubles. We are doing this here because in the tiny_gltf API, it's
    // implied we could have stored doubles. This is unrelated with the fact
    // that numbers in the JSON are read as doubles. Here we are talking about
    // the format where the data is stored inside the binary buffers that
    // comes with the glTF JSON part. Maybe remove the "double" types from
    // tiny_gltf?
    if (inverse_bind_matrices_component_size == sizeof(double)) {
      double temp[16], tempf[16];
      memcpy(
          temp,
          inverse_bind_matrices_data_start + i * inverse_bind_matrices_stride,
          inverse_bind_matrices_component_size * 16);
      for (int j = 0; j < 16; ++j) tempf[j] = float(temp[j]);
      inverse_bind_matrices[i] = glm::make_mat4(tempf);
    }
  }
}
