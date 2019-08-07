/*
MIT License

Copyright (c) 2019 Light Transport Entertainment Inc. And many contributors.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#include "insight-app.hh"
#include "json-util.hh"
#include "jsonrpc-http.hh"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// Include tinygltf's json.hpp
#include "json.hpp"

#include "fmt/core.h"

#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtx/string_cast.hpp"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "jsonrpc-command.hh"

#include <iostream>
#include <thread>
#include <vector>

using nlohmann::json;

namespace gltf_insight {

bool app::spawn_http_listen() {
#if !defined(GLTF_INSIGHT_WITH_JSONRPC)
  return false;
#else
  std::cout << "Start thread\n";

  _jsonrpc_exit_flag = false;

  std::function<void(const std::string&)> cb_f = [&](const std::string& msg) {
    bool ret = this->jsonrpc_dispatch(msg);
    // TODO(LTE): Check return value.
    (void)ret;
  };

  JSONRPC rpc;
  std::cout << "Listen...\n";
  bool ret = rpc.listen_blocking(cb_f, &_jsonrpc_exit_flag, _address, _port);
  std::cout << "Listen ret = " << ret << "\n";

  if (!ret) {
    _http_failed = true;
    return false;
  }

  return true;
#endif
}

#if 0
// req_len = -1 => allow arbitrary array length
static bool DecodeNumberArray(const json& j, int req_len,
                              std::vector<float>* values) {
  if (values == nullptr) {
    return false;
  }

  values->clear();

  if (!j.is_array()) {
    return false;
  }

  if (req_len > 0) {
    if (req_len != int(j.size())) {
      fmt::print("Array length must be {} but got {}.\n", req_len, j.size());
      return false;
    }
  }

  for (auto& elem : j) {
    if (!elem.is_number()) {
      std::cerr << "non-number value in an array found.\n";
      return false;
    }

    float value = float(elem.get<double>());

    values->push_back(value);
  }

  return true;
}
#endif

static bool DecodeMorphWeights(const json& j,
                               std::vector<std::pair<int, float>>* params) {
  params->clear();

  if (!j.is_array()) {
    std::cerr << "morph_weights must be an array object.\n";
    return false;
  }

  for (auto& elem : j) {
    if (!elem.count("target_id")) {
      std::cerr << "`target_id` is missing in morph_weight object.\n";
      continue;
    }

    json j_target_id = elem["target_id"];
    if (!j_target_id.is_number()) {
      std::cerr << "`target_id` must be a number value.\n";
      continue;
    }

    int target_id = j_target_id.get<int>();
    std::cout << "target_id " << target_id << "\n";

    if (!elem.count("weight")) {
      std::cerr << "`weight` is missing in morph_weight object.\n";
      continue;
    }

    json j_weight = elem["weight"];
    if (!j_weight.is_number()) {
      std::cerr << "`weight` must be a number value.\n";
      continue;
    }

    float weight = float(j_weight.get<double>());
    std::cout << "weight " << weight << "\n";

    params->push_back({target_id, weight});
  }

  return true;
}

static bool DecodeNodeTransforms(const json& j,
                                 std::vector<std::pair<int, Xform>>* params,
                                 bool additive = false) {
  params->clear();

  if (!j.is_array()) {
    std::cerr << "node_transforms must be an array object.\n";
    return false;
  }

  for (auto& elem : j) {
    if (!elem.count("joint_id")) {
      std::cerr << "`joint_id` is missing in node_transform object.\n";
      continue;
    }

    json j_joint_id = elem["joint_id"];
    if (!j_joint_id.is_number()) {
      std::cerr << "`joint_id` must be a number value.\n";
      continue;
    }

    int joint_id = j_joint_id.get<int>();
    std::cout << "joint_id " << joint_id << "\n";

    Xform xform;
    if (additive) {
      // initialize with zeros.

      // this is redundant, but just in case.
      xform.translation[0] = 0.0f;
      xform.translation[1] = 0.0f;
      xform.translation[2] = 0.0f;

      xform.rotation[0] = 0.0f;
      xform.rotation[1] = 0.0f;
      xform.rotation[2] = 0.0f;
      xform.rotation[3] = 0.0f;

      xform.scale[0] = 0.0f;
      xform.scale[1] = 0.0f;
      xform.scale[2] = 0.0f;
    }

    if (elem.count("translation")) {
      json j_translation = elem["translation"];

      std::vector<float> translation;
      if (!DecodeNumberArray(j_translation, /* length */ 3, &translation)) {
        std::cerr << "Failed to decode `translation` parameter.\n";
        continue;
      }

      xform.translation[0] = translation[0];
      xform.translation[1] = translation[1];
      xform.translation[2] = translation[2];

      fmt::print("joint_transform: translation = {}, {}, {}\n", translation[0],
                 translation[1], translation[2]);

    } else if (elem.count("scale")) {
      json j_scale = elem["scale"];

      std::vector<float> scale;
      if (!DecodeNumberArray(j_scale, /* length */ 3, &scale)) {
        std::cerr << "Failed to decode `scale` parameter.\n";
        continue;
      }

      xform.scale[0] = scale[0];
      xform.scale[1] = scale[1];
      xform.scale[2] = scale[2];

      fmt::print("joint_transform: scale = {}, {}, {}\n", scale[0], scale[1],
                 scale[2]);

    } else if (elem.count("rotation")) {  // quaternion
      json j_rotate = elem["rotation"];

      std::vector<float> rotate;
      if (!DecodeNumberArray(j_rotate, /* length */ 4, &rotate)) {
        std::cerr << "Failed to decode `rotate` parameter.\n";
        continue;
      }

      xform.rotation[0] = rotate[0];
      xform.rotation[1] = rotate[1];
      xform.rotation[2] = rotate[2];
      xform.rotation[3] = rotate[3];

      fmt::print("joint_transform: rotation = {}, {}, {}, {}\n", rotate[0],
                 rotate[1], rotate[2], rotate[3]);

    } else if (elem.count("rotation_angle")) {  // euler XYZ
      json j_rotate = elem["rotation_angle"];

      std::vector<float> angle;
      if (!DecodeNumberArray(j_rotate, /* length */ 3, &angle)) {
        std::cerr << "Failed to decode `rotate_angle` parameter.\n";
        continue;
      }

      // to quaternion. value is in radian!
      glm::quat q =
          glm::quat(glm::vec3(glm::radians(angle[0]), glm::radians(angle[1]),
                              glm::radians(angle[2])));

      xform.rotation[0] = q.x;
      xform.rotation[1] = q.y;
      xform.rotation[2] = q.z;
      xform.rotation[3] = q.w;

      fmt::print(
          "joint_transform: rotation_angle {}, {}, {}, quat = {}, {}, {}, {}\n",
          angle[0], angle[1], angle[2], q.x, q.y, q.z, q.w);
    }

    params->push_back({joint_id, xform});
  }

  return true;
}

bool app::jsonrpc_dispatch(const std::string& json_str) {
  json j;

  try {
    j = json::parse(json_str);
  } catch (const std::exception& e) {
    fmt::print("Invalid JSON string. what = {}\n", e.what());
    return false;
  }

  if (!j.is_object()) {
    std::cerr << "Invalid JSON message.\n";
    return false;
  }

  if (!j.count("jsonrpc")) {
    std::cerr << "JSON message does not contain `jsonrpc`.\n";
    return false;
  }

  if (!j["jsonrpc"].is_string()) {
    std::cerr << "value for `jsonrpc` must be string.\n";
    return false;
  }

  std::string version = j["jsonrpc"].get<std::string>();
  if (version.compare("2.0") != 0) {
    std::cerr << "JSONRPC version must be \"2.0\" but got \"" << version
              << "\"\n";
    return false;
  }

  if (!j.count("method")) {
    std::cerr << "JSON message does not contain `method`.\n";
    return false;
  }

  if (!j["method"].is_string()) {
    std::cerr << "`method` must be string.\n";
    return false;
  }

  std::string method = j["method"].get<std::string>();
  if (method.compare("update") != 0) {
    std::cerr << "`method` must be `update`, but got `" << method << "'\n";
    return false;
  }

  if (!j.count("params")) {
    std::cerr << "JSON message does not contain `params`.\n";
    return false;
  }

  json params = j["params"];

  if (params.count("morph_weights")) {
    json morph_weights = params["morph_weights"];

    std::vector<std::pair<int, float>> morph_params;
    bool ret = DecodeMorphWeights(morph_weights, &morph_params);
    if (ret) {
      std::cout << "Update morph_weights " << morph_weights << "\n";
    }

    // Add update request to command queue, since directly update
    // gltf_scene_tree from non-main thread will cause segmentation fault.
    // Updating the scene must be done in main thead.
    {
      std::lock_guard<std::mutex> lock(_command_queue_mutex);

      for (size_t i = 0; i < morph_params.size(); i++) {
        Command command;
        command.type = Command::MORPH_WEIGHT;
        command.morph_weight = morph_params[i];

        _command_queue.push(command);
      }
    }

  } else if (params.count("joint_transforms")) {
    json joint_transforms = params["joint_transforms"];

    std::vector<std::pair<int, Xform>> transform_params;
    bool ret = DecodeNodeTransforms(joint_transforms, &transform_params);
    if (ret) {
      std::cout << "Update joint_transforms " << joint_transforms << "\n";
    }

    // Add update request to command queue, since directly update
    // gltf_scene_tree from non-main thread will cause segmentation fault.
    // Updating the scene must be done in main thead.
    {
      std::lock_guard<std::mutex> lock(_command_queue_mutex);

      for (size_t i = 0; i < joint_transforms.size(); i++) {
        Command command;
        command.type = Command::JOINT_TRANSFORM;
        command.joint_transform = transform_params[i];

        _command_queue.push(command);
      }
    }

  } else if (params.count("additive_joint_transforms")) {
    json joint_transforms = params["additive_joint_transforms"];

    std::vector<std::pair<int, Xform>> transform_params;
    bool ret = DecodeNodeTransforms(joint_transforms, &transform_params,
                                    /* additive */ true);
    if (ret) {
      std::cout << "Update additive_joint_transforms " << joint_transforms
                << "\n";
    }

    // Add update request to command queue, since directly update
    // gltf_scene_tree from non-main thread will cause segmentation fault.
    // Updating the scene must be done in main thead.
    {
      std::lock_guard<std::mutex> lock(_command_queue_mutex);

      for (size_t i = 0; i < joint_transforms.size(); i++) {
        Command command;
        command.type = Command::ADDITIVE_JOINT_TRANSFORM;
        command.joint_transform = transform_params[i];

        _command_queue.push(command);
      }
    }
  }

  return true;
}

bool app::update_scene(const Command& command) {
  if (command.type == Command::MORPH_WEIGHT) {
    std::pair<int, float> param = command.morph_weight;

    if (gltf_scene_tree.pose.blend_weights.size() > 0) {
      int idx = param.first;

      std::cout << idx << ", # of morphs = "
                << gltf_scene_tree.pose.blend_weights.size() << "\n";

      if ((idx >= 0) &&
          (idx < int(gltf_scene_tree.pose.blend_weights.size()))) {
        float weight = std::min(1.0f, std::max(0.0f, param.second));

        std::cout << "Update " << idx << "th morph_weight with " << weight
                  << "\n";
        gltf_scene_tree.pose.blend_weights[size_t(idx)] = weight;
      }
    }

  } else if (command.type == Command::JOINT_TRANSFORM) {
    std::pair<int, Xform> param = command.joint_transform;

    int joint_id = param.first;
    const Xform& xform = param.second;

    if ((joint_id >= 0) &&
        (joint_id < int(loaded_meshes[0].flat_joint_list.size()))) {
      gltf_node* joint = loaded_meshes[0].flat_joint_list[size_t(joint_id)];

      //
      // Transform is given by world space.
      // Convert it to local space(relative to parent's bone)
      //
      // FIXME(LTE): Refactor code.
      //
      glm::mat4 parent_bone_world_xform(1.f);
      if (joint->parent) {
        parent_bone_world_xform = joint->parent->world_xform;
      }

      glm::mat4 bone_world_xform;
      // Compute the world transform
      mesh* a_mesh =
          &loaded_meshes[size_t(joint->skin_mesh_node->gltf_mesh_id)];
      gltf_node* mesh_node =
          gltf_scene_tree.get_node_with_index(a_mesh->instance.node);

      const auto joint_index = size_t(
          a_mesh->joint_inverse_bind_matrix_map.at(joint->gltf_node_index));

      glm::mat4 bind_matrix =
          glm::inverse(a_mesh->inverse_bind_matrices[joint_index]);
      glm::mat4 joint_matrix = a_mesh->joint_matrices[joint_index];

      bone_world_xform = mesh_node->world_xform * joint_matrix * bind_matrix;

      glm::mat4 new_bone_world_xform;
      {
        glm::vec3 tx(xform.translation[0], xform.translation[1],
                     xform.translation[2]);
        // (w, x, y, z)
        glm::quat q(xform.rotation[3], xform.rotation[0], xform.rotation[1],
                    xform.rotation[2]);
        glm::vec3 sc(xform.scale[0], xform.scale[1], xform.scale[2]);

        // T = trans x rot x scale
        new_bone_world_xform = glm::translate(glm::mat4(1.0f), tx) *
                               glm::mat4_cast(q) *
                               glm::scale(glm::mat4(1.0f), sc);

        // std::cout << "new_bone_world_xform = " <<
        // glm::to_string(new_bone_world_xform) << "\n";
      }

      // The pose is expressed in the world referential, but skeleton and
      // manipulator are in the mesh's referential. The skeleton and mesh don't
      // necessary line-up!
      auto currently_posed =
          glm::inverse(parent_bone_world_xform) * joint->world_xform *
          glm::inverse(bone_world_xform) * new_bone_world_xform;

      {
        // Get a local tx/rot/scale
        glm::vec3 translation(0.f), scale(1.f), skew(1.f);
        // glm::quat is (w, x, y, z)
        glm::quat rotation(1.f, 0.f, 0.f, 0.f);
        glm::vec4 perspective(1.f);
        glm::decompose(currently_posed, scale, rotation, translation, skew,
                       perspective);

        // Update pose with local xform.
        joint->pose.translation = translation;
        joint->pose.rotation = rotation;
        joint->pose.scale = scale;

        fmt::print(
            "Update joint[{}]. local. T = {}, {}, {}, R = {}, {}, {}, {}, S = "
            "{}, {}, {}\n",
            joint_id, translation[0], translation[1], translation[2],
            rotation[0], rotation[1], rotation[2], rotation[3], scale[0],
            scale[1], scale[2]);

        // Update mesh.
        // TODO(LTE): Call this function after consuming all commands from
        // queue.
        update_mesh_skeleton_graph_transforms(gltf_scene_tree);

        int active_joint_gltf_node = -1;
        bool gpu_geometry_buffers_dirty =
            false;  // FIXME(LTE): Look up CPU skinning mode of GUI
        update_geometry(gpu_geometry_buffers_dirty, active_joint_gltf_node);
      }

    } else {
      fmt::print("Invalid joint ID {}. joints.size = {}\n", joint_id,
                 loaded_meshes[0].flat_joint_list.size());
    }

  } else if (command.type == Command::ADDITIVE_JOINT_TRANSFORM) {
    std::pair<int, Xform> param = command.joint_transform;

    int joint_id = param.first;
    const Xform& xform = param.second;

    if ((joint_id >= 0) &&
        (joint_id < int(loaded_meshes[0].flat_joint_list.size()))) {
      gltf_node* joint = loaded_meshes[0].flat_joint_list[size_t(joint_id)];

      //
      // Transform is given by world space.
      // Convert it to local space(relative to parent's bone)
      //
      // FIXME(LTE): Refactor code.
      //
      glm::mat4 parent_bone_world_xform(1.f);
      if (joint->parent) {
        parent_bone_world_xform = joint->parent->world_xform;
      }

      glm::mat4 bone_world_xform;
      // Compute the world transform
      mesh* a_mesh =
          &loaded_meshes[size_t(joint->skin_mesh_node->gltf_mesh_id)];
      gltf_node* mesh_node =
          gltf_scene_tree.get_node_with_index(a_mesh->instance.node);

      const auto joint_index = size_t(
          a_mesh->joint_inverse_bind_matrix_map.at(joint->gltf_node_index));

      glm::mat4 bind_matrix =
          glm::inverse(a_mesh->inverse_bind_matrices[joint_index]);
      glm::mat4 joint_matrix = a_mesh->joint_matrices[joint_index];

      bone_world_xform = mesh_node->world_xform * joint_matrix * bind_matrix;

      glm::mat4 new_bone_world_xform;
      {
        glm::vec3 curr_tx(0.f), curr_sc(1.f), skew(1.f);
        // glm::quat is (w, x, y, z)
        glm::quat curr_quat(1.f, 0.f, 0.f, 0.f);
        glm::vec4 perspective(1.f);
        glm::decompose(bone_world_xform, curr_sc, curr_quat, curr_tx, skew,
                       perspective);

        glm::vec3 tx(xform.translation[0], xform.translation[1],
                     xform.translation[2]);
        // (w, x, y, z)
        glm::quat q(xform.rotation[3], xform.rotation[0], xform.rotation[1],
                    xform.rotation[2]);
        glm::vec3 sc(xform.scale[0], xform.scale[1], xform.scale[2]);

        // FIXME(LTE): additive rotation.
        new_bone_world_xform = glm::translate(glm::mat4(1.0f), tx + curr_tx) *
                           glm::mat4_cast(q + curr_quat) * glm::scale(glm::mat4(1.0f), sc + curr_sc);

      }

      // The pose is expressed in the world referential, but skeleton and
      // manipulator are in the mesh's referential. The skeleton and mesh don't
      // necessary line-up!
      auto currently_posed =
          glm::inverse(parent_bone_world_xform) * joint->world_xform *
          glm::inverse(bone_world_xform) * new_bone_world_xform;

      //std::cout << "currently_posed = " << glm::to_string(currently_posed)
      //          << "\n";

      //std::cout << "parent bone world xform : "
      //          << glm::to_string(parent_bone_world_xform) << "\n";
      //std::cout << "joint.world_xform : " << glm::to_string(joint->world_xform)
      //          << "\n";
      //std::cout << "bone_world_xform" << glm::to_string(bone_world_xform)
      //          << "\n";
      //std::cout << "new_bone_world_xform"
      //          << glm::to_string(new_bone_world_xform) << "\n";

      {
        // Get a local tx/rot/scale
        glm::vec3 translation(0.f), scale(1.f), skew(1.f);
        // glm::quat is (w, x, y, z)
        glm::quat rotation(1.f, 0.f, 0.f, 0.f);
        glm::vec4 perspective(1.f);
        glm::decompose(currently_posed, scale, rotation, translation, skew,
                       perspective);

        joint->pose.translation = translation;
        joint->pose.rotation = rotation;
        joint->pose.scale = scale;

        fmt::print(
            "Additively update joint[{}]. T = {}, {}, {}, R = {}, {}, {}, {}, "
            "S = "
            "{}, {}, {}\n",
            joint_id, translation[0], translation[1], translation[2],
            rotation[0], rotation[1], rotation[2], rotation[3], scale[0],
            scale[1], scale[2]);

        // Update mesh.
        // TODO(LTE): Call this function after consuming all commands from
        // queue.
        update_mesh_skeleton_graph_transforms(gltf_scene_tree);

        int active_joint_gltf_node = -1;
        bool gpu_geometry_buffers_dirty =
            false;  // FIXME(LTE): Look up CPU skinning mode of GUI state.
        update_geometry(gpu_geometry_buffers_dirty, active_joint_gltf_node);
      }

      // Update scene
      // TODO(LTE): Call this function after consuming all commands from queue.
      update_mesh_skeleton_graph_transforms(gltf_scene_tree);

      int active_joint_gltf_node = -1;
      bool gpu_geometry_buffers_dirty =
          false;  // FIXME(LTE): Look up CPU skinning mode?
      update_geometry(gpu_geometry_buffers_dirty, active_joint_gltf_node);
    } else {
      fmt::print("Invalid joint ID {}. joints.size = {}\n", joint_id,
                 loaded_meshes[0].flat_joint_list.size());
    }
  }

  return true;
}

}  // namespace gltf_insight
