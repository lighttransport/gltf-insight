#pragma once

#include <array>
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
      apply_pose();
    }
  }

  void set_time(float time) {
    current_time = glm::clamp(time, min_time, max_time);
  }

  void compute_time_boundaries() {
    for (const auto& sampler : samplers) {
      min_time = std::min(min_time, sampler.min_v);
      max_time = std::max(max_time, sampler.max_v);
    }
  }

  // just apply the lower keyframe state
  void apply_step(const channel& chan, int lower_keyframe) {
    switch (chan.mode) {
      case channel::path::weight:
        break;
      case channel::path::translation:
        chan.target_graph_node->pose.translation =
            chan.keyframes[lower_keyframe].second.motion.translation;
        break;
      case channel::path::scale:
        chan.target_graph_node->pose.scale =
            chan.keyframes[lower_keyframe].second.motion.scale;
      case channel::path::rotation:
        chan.target_graph_node->pose.rotation =
            chan.keyframes[lower_keyframe].second.motion.rotation;
      default:
        break;
    }
  }

  // just glm::mix all of the components for vectors, and slerp for quaternions?
  void apply_linear(const channel& chan, int lower_keyframe, int upper_keyframe,
                    float mix) {
    switch (chan.mode) {
      case channel::path::weight:
        // TODO weight
        break;
      case channel::path::translation: {
        glm::vec3 lower_translation =
            chan.keyframes[lower_keyframe].second.motion.translation;
        glm::vec3 upper_translation =
            chan.keyframes[upper_keyframe].second.motion.translation;
        glm::vec3 result = glm::mix(lower_translation, upper_translation, mix);

        chan.target_graph_node->pose.translation = result;
      } break;
      case channel::path::scale: {
        glm::vec3 lower_scale =
            chan.keyframes[lower_keyframe].second.motion.scale;
        glm::vec3 upper_scale =
            chan.keyframes[upper_keyframe].second.motion.scale;
        glm::vec3 result = glm::mix(lower_scale, upper_scale, mix);

        chan.target_graph_node->pose.scale = result;
      } break;
      case channel::path::rotation: {
        glm::quat lower_rotation =
            chan.keyframes[lower_keyframe].second.motion.rotation;
        glm::quat upper_rotation =
            chan.keyframes[upper_keyframe].second.motion.rotation;
        glm::quat result = glm::slerp(lower_rotation, upper_rotation, mix);

        chan.target_graph_node->pose.rotation = glm::normalize(result);

      } break;
      // nothing to do here for us in this case...
      default:
        break;
    }
  }

  void apply_channel_target_for_interpolation_value(float interpolation_value,
                                                    sampler::interpolation mode,
                                                    int lower_frame,
                                                    int upper_frame,
                                                    const channel& chan) {
    if (!chan.target_graph_node) return;
    switch (mode) {
      case sampler::interpolation::step:
        apply_step(chan, lower_frame);
        break;
      case sampler::interpolation::linear:
        apply_linear(chan, lower_frame, upper_frame, interpolation_value);
        break;
      case sampler::interpolation::cubic_spline:
        // TODO implement cubic spline...
      default:
        break;
    }
  }

  void apply_pose() {
    for (auto& channel : channels) {
      const auto& sampler = samplers[channel.sampler_index];

      // TODO probably a special case when animation has only one keyframe : see
      // https://github.com/KhronosGroup/glTF/issues/1597

      // check that current time is indeed findable somewhere in this sampler
      assert(current_time >= sampler.min_v && current_time <= sampler.max_v);

      std::array<std::pair<int, float>, 2> keyframe_interval;
      bool found = false;

      for (size_t frame = 0; frame < sampler.keyframes.size() - 1; frame++) {
        keyframe_interval[0] = sampler.keyframes[frame];
        keyframe_interval[1] = sampler.keyframes[frame + 1];

        if (keyframe_interval[0].second <= current_time &&
            keyframe_interval[1].second >= current_time) {
          found = true;
          break;
        }
      }

      if (found) {
        const int lower_frame = keyframe_interval[0].first;
        const int upper_frame = keyframe_interval[1].first;
        const float lower_time = keyframe_interval[0].second;
        const float upper_time = keyframe_interval[1].second;

        // current_time is a value between [lower_time; upper_time]
        // we want to get a value between 0 and 1
        const float interpolation_value =
            (current_time - lower_time) / (upper_time - lower_time);

        // knowing what to interpolate, the interpolation method to use, the 2
        // keyframe index and the interpolation value, we can then calculate
        // and apply the channel target

        apply_channel_target_for_interpolation_value(interpolation_value,
                                                     sampler.mode, lower_frame,
                                                     upper_frame, channel);
      }
    }
  }

  void set_playing_state(bool state = true) { playing = state; }

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
