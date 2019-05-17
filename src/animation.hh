#pragma once

#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <utility>
#include <vector>

#include "gltf-graph.hh"

struct animation {
  struct channel {
    struct keyframe_content {
      union {
        glm::vec3 translation;
        glm::vec3 scale;
        glm::quat rotation;
        float weight;
      } motion;

      keyframe_content() { std::memset(this, 0, sizeof(keyframe_content)); }
    };

    enum class path : uint8_t {
      not_assigned,
      translation,
      scale,
      rotation,
      weight
    };

    std::vector<std::pair<int, keyframe_content>> keyframes;
    int sampler_index;
    int target_node;
    gltf_node* target_graph_node;
    path mode;

    channel()
        : sampler_index(0),
          target_node(-1),
          target_graph_node(nullptr),
          mode(path::not_assigned) {}
  };

  struct sampler {
    enum class interpolation : uint8_t {
      not_assigned,
      step,
      linear,
      cubic_spline
    };
    std::vector<std::pair<int, float>> keyframes;
    interpolation mode;
    float min_v, max_v;
    sampler() : mode(interpolation::not_assigned), min_v(0), max_v(0) {}
  };

  std::vector<channel> channels;
  std::vector<sampler> samplers;

  float current_time;
  float min_time, max_time;

  bool playing;

  std::string name;

  void add_time(float delta) {
    current_time += delta;
    if (current_time > max_time)
      current_time = min_time + current_time - max_time;

    if (playing) {
      // TODO apply animation pose
    }
  }

  void compute_time_boundaries() {
    for (const auto& sampler : samplers) {
      min_time = std::min(min_time, sampler.min_v);
      max_time = std::max(max_time, sampler.max_v);
    }
  }

  animation()
      : current_time(0), min_time(0), max_time(0), playing(false), name() {}

  void set_gltf_graph_targets(gltf_node* root_node) {
    for (auto& channel : channels)
      if (channel.target_node >= 0) {
        gltf_node* node = root_node->get_node_with_index(channel.target_node);
        if (node) channel.target_graph_node = node;
      }
  }
};
