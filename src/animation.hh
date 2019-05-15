#pragma once

#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstring>

struct animation {
  struct channel {
    struct keyframe_content {
      union {
        glm::vec3 translation;
        glm::vec3 scale;
        glm::quat rotation;
        float weight;
      } motion;

      enum class path : uint8_t { not_assigned, translation, scale, rotation, weight };
      path mode;
      int target;

      keyframe_content() { std::memset(this, 0, sizeof(keyframe_content)); }
    };

    std::vector<std::pair<int, keyframe_content>> keyframes;
  };

  struct sampler {
    enum class interpolation : uint8_t { not_assigned, step, linear, cubic_spline };
    std::vector<std::pair<int, float>> keyframes;
    int input, output;
    interpolation mode;
    sampler() : input(0), output(0), mode(interpolation::not_assigned) {}
  };

  std::vector<channel> channels;
  std::vector<sampler> samplers;

  float current_time;
  float min_time, max_time;

  bool playing;

  void add_time(float delta) {
    current_time += delta;
    if (current_time > max_time)
      current_time = min_time + current_time - max_time;

    if (playing) {
      // TODO apply animation pose
    }
  }

  animation() : current_time(0), min_time(0), max_time(0), playing(false) {}
};
