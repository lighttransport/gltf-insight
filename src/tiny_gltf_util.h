//
// TinyGLTF utility functions
//
//
// The MIT License (MIT)
//
// Copyright (c) 2015 - 2018 Syoyo Fujita, Aurélien Chatelain and many
// contributors.
//

#pragma once

#include <iostream>

#include "tiny_gltf.h"

inline bool has_mesh(const tinygltf::Node& node) { return node.mesh >= 0; }

inline std::vector<int> get_all_mesh_nodes_indices(
    const tinygltf::Model& model, const tinygltf::Scene& scene) {
  std::vector<int> output;
  const auto& node_list = scene.nodes;

  for (auto node_index : node_list) {
    if (has_mesh(model.nodes[size_t(node_index)])) output.push_back(node_index);
  }

  return output;
}

inline int find_node_with_mesh_in_children(const tinygltf::Model& model,
                                           int root) {
  const auto& root_node = model.nodes[size_t(root)];
  if (has_mesh(root_node)) return root;

  for (auto child : root_node.children) {
    const auto result = find_node_with_mesh_in_children(model, child);
    if (result > 0 && model.nodes[size_t(result)].mesh >= 0) return result;
  }

  return -1;
}

inline int find_main_scene(const tinygltf::Model& model) {
  return model.defaultScene >= 0 ? model.defaultScene : 0;
}

inline int find_main_mesh_node(const tinygltf::Model& model) {
  const auto& node_list = model.scenes[size_t(find_main_scene(model))].nodes;

  for (auto node : node_list) {
    const auto mesh_node = find_node_with_mesh_in_children(model, node);
    if (mesh_node >= 0) return mesh_node;
  }

  return -1;
}

namespace tinygltf {

namespace util {

inline std::string PrintMode(int mode) {
  if (mode == TINYGLTF_MODE_POINTS) {
    return "POINTS";
  } else if (mode == TINYGLTF_MODE_LINE) {
    return "LINE";
  } else if (mode == TINYGLTF_MODE_LINE_LOOP) {
    return "LINE_LOOP";
  } else if (mode == TINYGLTF_MODE_TRIANGLES) {
    return "TRIANGLES";
  } else if (mode == TINYGLTF_MODE_TRIANGLE_FAN) {
    return "TRIANGLE_FAN";
  } else if (mode == TINYGLTF_MODE_TRIANGLE_STRIP) {
    return "TRIANGLE_STRIP";
  }
  return "**UNKNOWN**";
}

inline std::string PrintTarget(int target) {
  if (target == 34962) {
    return "GL_ARRAY_BUFFER";
  } else if (target == 34963) {
    return "GL_ELEMENT_ARRAY_BUFFER";
  } else {
    return "**UNKNOWN**";
  }
}

inline std::string PrintType(int ty) {
  if (ty == TINYGLTF_TYPE_SCALAR) {
    return "SCALAR";
  } else if (ty == TINYGLTF_TYPE_VECTOR) {
    return "VECTOR";
  } else if (ty == TINYGLTF_TYPE_VEC2) {
    return "VEC2";
  } else if (ty == TINYGLTF_TYPE_VEC3) {
    return "VEC3";
  } else if (ty == TINYGLTF_TYPE_VEC4) {
    return "VEC4";
  } else if (ty == TINYGLTF_TYPE_MATRIX) {
    return "MATRIX";
  } else if (ty == TINYGLTF_TYPE_MAT2) {
    return "MAT2";
  } else if (ty == TINYGLTF_TYPE_MAT3) {
    return "MAT3";
  } else if (ty == TINYGLTF_TYPE_MAT4) {
    return "MAT4";
  }
  return "**UNKNOWN**";
}

inline std::string PrintComponentType(int ty) {
  if (ty == TINYGLTF_COMPONENT_TYPE_BYTE) {
    return "BYTE";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    return "UNSIGNED_BYTE";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_SHORT) {
    return "SHORT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    return "UNSIGNED_SHORT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_INT) {
    return "INT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
    return "UNSIGNED_INT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    return "FLOAT";
  } else if (ty == TINYGLTF_COMPONENT_TYPE_DOUBLE) {
    return "DOUBLE";
  }

  return "**UNKNOWN**";
}

inline int GetAnimationSamplerInputCount(
    const tinygltf::AnimationSampler& sampler, const tinygltf::Model& model) {
  const tinygltf::Accessor& accessor = model.accessors[size_t(sampler.input)];
  return int(accessor.count);
}

inline int GetAnimationSamplerOutputCount(
    const tinygltf::AnimationSampler& sampler, const tinygltf::Model& model) {
  const tinygltf::Accessor& accessor = model.accessors[size_t(sampler.output)];
  return int(accessor.count);
}

inline bool GetAnimationSamplerInputMinMax(
    const tinygltf::AnimationSampler& sampler, const tinygltf::Model& model,
    float* min_value, float* max_value) {
  const tinygltf::Accessor& accessor = model.accessors[size_t(sampler.input)];

  // Assume scalar value.
  if ((accessor.minValues.size() > 0) && (accessor.maxValues.size() > 0)) {
    (*min_value) = float(accessor.minValues[0]);
    (*max_value) = float(accessor.maxValues[0]);
    return true;
  } else {
    (*min_value) = 0.0f;
    (*max_value) = 0.0f;
    return false;
  }
}

// Utility function for decoding animation value
inline float DecodeAnimationChannelValue(int8_t c) {
  return std::max(float(c) / 127.0f, -1.0f);
}
inline float DecodeAnimationChannelValue(uint8_t c) {
  return float(c) / 255.0f;
}
inline float DecodeAnimationChannelValue(int16_t c) {
  return std::max(float(c) / 32767.0f, -1.0f);
}
inline float DecodeAnimationChannelValue(uint16_t c) {
  return float(c) / 65525.0f;
}

inline const uint8_t* GetBufferAddress(const int i, const Accessor& accessor,
                                       const BufferView& bufferViewObject,
                                       const Buffer& buffer) {
  if (i >= int(accessor.count)) return nullptr;

  int byte_stride = accessor.ByteStride(bufferViewObject);
  if (byte_stride == -1) {
    return nullptr;
  }

  // TODO(syoyo): Bounds check.
  const uint8_t* base_addr =
      buffer.data.data() + bufferViewObject.byteOffset + accessor.byteOffset;
  const uint8_t* addr = base_addr + i * byte_stride;
  return addr;
}

inline bool DecodeScalarAnimationValue(const size_t i,
                                       const tinygltf::Accessor& accessor,
                                       const tinygltf::Model& model,
                                       float* scalar) {
  const BufferView& bufferView = model.bufferViews[size_t(accessor.bufferView)];
  const Buffer& buffer = model.buffers[size_t(bufferView.buffer)];

  const uint8_t* addr = GetBufferAddress(int(i), accessor, bufferView, buffer);
  if (addr == nullptr) {
    std::cerr << "Invalid glTF data?" << std::endl;
    return false;
  }

  float value = 0.0f;

  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
    value =
        DecodeAnimationChannelValue(*(reinterpret_cast<const int8_t*>(addr)));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    value =
        DecodeAnimationChannelValue(*(reinterpret_cast<const uint8_t*>(addr)));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
    value =
        DecodeAnimationChannelValue(*(reinterpret_cast<const int16_t*>(addr)));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    value =
        DecodeAnimationChannelValue(*(reinterpret_cast<const uint16_t*>(addr)));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    value = *(reinterpret_cast<const float*>(addr));
  } else {
    std::cerr << "??? Unknown componentType : "
              << PrintComponentType(accessor.componentType) << std::endl;
    return false;
  }

  (*scalar) = value;

  return true;
}

inline bool DecodeTranslationAnimationValue(const size_t i,
                                            const tinygltf::Accessor& accessor,
                                            const tinygltf::Model& model,
                                            float* xyz) {
  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
    std::cerr << "`translation` must be float type." << std::endl;
    return false;
  }

  const BufferView& bufferView = model.bufferViews[size_t(accessor.bufferView)];
  const Buffer& buffer = model.buffers[size_t(bufferView.buffer)];

  const uint8_t* addr = GetBufferAddress(int(i), accessor, bufferView, buffer);
  if (addr == nullptr) {
    std::cerr << "Invalid glTF data?" << std::endl;
    return 0.0f;
  }

  const float* ptr = reinterpret_cast<const float*>(addr);

  xyz[0] = *(ptr + 0);
  xyz[1] = *(ptr + 1);
  xyz[2] = *(ptr + 2);

  return true;
}

inline bool DecodeScaleAnimationValue(const size_t i,
                                      const tinygltf::Accessor& accessor,
                                      const tinygltf::Model& model,
                                      float* xyz) {
  if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) {
    std::cerr << "`scale` must be float type." << std::endl;
    return false;
  }

  const BufferView& bufferView = model.bufferViews[size_t(accessor.bufferView)];
  const Buffer& buffer = model.buffers[size_t(bufferView.buffer)];

  const uint8_t* addr = GetBufferAddress(int(i), accessor, bufferView, buffer);
  if (addr == nullptr) {
    std::cerr << "Invalid glTF data?" << std::endl;
    return 0.0f;
  }

  const float* ptr = reinterpret_cast<const float*>(addr);

  xyz[0] = *(ptr + 0);
  xyz[1] = *(ptr + 1);
  xyz[2] = *(ptr + 2);

  return true;
}

inline bool DecodeRotationAnimationValue(const size_t i,
                                         const tinygltf::Accessor& accessor,
                                         const tinygltf::Model& model,
                                         float* xyzw) {
  const BufferView& bufferView = model.bufferViews[size_t(accessor.bufferView)];
  const Buffer& buffer = model.buffers[size_t(bufferView.buffer)];

  const uint8_t* addr = GetBufferAddress(int(i), accessor, bufferView, buffer);
  if (addr == nullptr) {
    std::cerr << "Invalid glTF data?" << std::endl;
    return false;
  }

  if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_BYTE) {
    xyzw[0] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int8_t*>(addr) + 0));
    xyzw[1] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int8_t*>(addr) + 1));
    xyzw[2] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int8_t*>(addr) + 2));
    xyzw[3] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int8_t*>(addr) + 3));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
    xyzw[0] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint8_t*>(addr) + 0));
    xyzw[1] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint8_t*>(addr) + 1));
    xyzw[2] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint8_t*>(addr) + 2));
    xyzw[3] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint8_t*>(addr) + 3));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_SHORT) {
    xyzw[0] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int16_t*>(addr) + 0));
    xyzw[1] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int16_t*>(addr) + 1));
    xyzw[2] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int16_t*>(addr) + 2));
    xyzw[3] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const int16_t*>(addr) + 3));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
    xyzw[0] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint16_t*>(addr) + 0));
    xyzw[1] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint16_t*>(addr) + 1));
    xyzw[2] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint16_t*>(addr) + 2));
    xyzw[3] = DecodeAnimationChannelValue(
        *(reinterpret_cast<const uint16_t*>(addr) + 3));
  } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
    xyzw[0] = *(reinterpret_cast<const float*>(addr) + 0);
    xyzw[1] = *(reinterpret_cast<const float*>(addr) + 1);
    xyzw[2] = *(reinterpret_cast<const float*>(addr) + 2);
    xyzw[3] = *(reinterpret_cast<const float*>(addr) + 3);
  } else {
    std::cerr << "??? Unknown componentType : "
              << PrintComponentType(accessor.componentType) << std::endl;
    return false;
  }

  return true;
}

}  // namespace util

}  // namespace tinygltf
