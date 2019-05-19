#pragma once

#include <array>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <utility>
#include <vector>

#include "gltf-graph.hh"

/// Represent a loaded animation
struct animation {
  /// Represent a glTF animation channel
  struct channel {
    /// Represent the keyframe array content.
    struct keyframe_content {
      // To keeyp the same object, but to lower the memory footprint, we are
      // using an union here
      union {
        glm::vec3 translation;
        glm::vec3 scale;
        glm::quat rotation;
        float weight;
      } motion;

      keyframe_content() { std::memset(this, 0, sizeof(keyframe_content)); }
    };

    /// Types of keyframes
    enum class path : uint8_t {
      not_assigned,
      translation,
      scale,
      rotation,
      weight
    };

    /// Storage for keyframes, they are stored with their frame index
    std::vector<std::pair<int, keyframe_content>> keyframes;

    /// Index of the sampler to be used
    int sampler_index;

    /// glTF index of the node being moved by this
    int target_node;
    /// Pointer to the mesh/skeleton graph inside gltf-insight used to display
    gltf_node* target_graph_node;
    path mode;

    /// Set everything to an "unassigned" value or equivalent
    channel()
        : sampler_index(-1),
          target_node(-1),
          target_graph_node(nullptr),
          mode(path::not_assigned) {}
  };

  /// Represent a gltf sampler
  struct sampler {
    enum class interpolation : uint8_t {
      not_assigned,
      step,
      linear,
      cubic_spline
    };

    /// Keyframe storage. they are in the [frame_index, time_point] format
    std::vector<std::pair<int, float>> keyframes;

    /// How to interpolate the frames between two keyframes
    interpolation mode;
    /// Min and max values
    float min_v, max_v;
    /// Set everything to "not assigned"
    sampler()
        : mode(interpolation::not_assigned),
          min_v(0),
          max_v(std::numeric_limits<float>::max()) {}
  };

  /// Data
  std::vector<channel> channels;
  std::vector<sampler> samplers;

  /// Timing
  float current_time;
  float min_time, max_time;

  /// Set to true when playing
  bool playing;

  /// Name of the animation
  std::string name;

  void set_playing_state(bool state = true) { playing = state; }

  /// Add time forward
  void add_time(float delta) {
    if (current_time < min_time) current_time = min_time;
    current_time += delta;
    if (current_time > max_time)
      current_time = min_time + current_time - max_time;

    if (playing) {
      apply_pose();
    }
  }

  /// Set the animation to the specified timepoint
  void set_time(float time) {
    current_time = glm::clamp(time, min_time, max_time);
  }

  /// Checks the minimal and maximal times in samplers
  void compute_time_boundaries() {
    max_time = std::numeric_limits<float>::max();
    for (const auto& sampler : samplers) {
      min_time = std::max(min_time, sampler.min_v);
      max_time = std::min(max_time, sampler.max_v);
    }

    // TODO I think I've read something about time not starting at zero for
    // animations. Find how we are supposed to actually handle this
    // if (min_time != 0) {
    //  const auto delta = min_time;
    //  min_time -= delta;
    //  max_time -= delta;
    //  for (auto& sampler : samplers) {
    //    sampler.min_v -= delta;
    //    sampler.max_v -= delta;
    //    for (auto& frame : sampler.keyframes) frame.second -= delta;
    //  }
    //}
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

 private:
  // just apply the lower keyframe state
  void apply_step(const channel& chan, int lower_keyframe) {
    switch (chan.mode) {
      case channel::path::weight:
        // TODO morph wieghts
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

  // See
  // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#appendix-c-spline-interpolation
  template <typename T>
  T cubic_spline_interpolate(float t, T p0, T m0, T p1, T m1) {
    const auto t2 = t * t;
    const auto t3 = t2 * t;

    return (2 * t3 - 3 * t2 + 1) * p0 + (t3 - 2 * t2 + t) * m0 +
           (-2 * t3 + 3 * t2) * p1 + (t3 - t2) * m1;
  }

  void apply_cubic_spline(float interpolation_value, int lower_frame,
                          int upper_frame, float lower_time, float upper_time,
                          const channel& chan) {
    // Retrive data
    const auto p0 = chan.keyframes[3 * lower_frame + 1];
    const auto no_scale_m0 = chan.keyframes[3 * lower_frame + 2];
    const auto no_scale_m1 = chan.keyframes[3 * upper_frame + 0];
    const auto p1 = chan.keyframes[3 * upper_frame + 1];
    // m0 and m1 need to be scaled by this value (delta between the 2 keyframes
    // being interpolated)
    const auto delta = upper_time - lower_time;

    switch (chan.mode) {
      case channel::path::weight:
        // TODO morph weights
        break;
      case channel::path::translation: {
        chan.target_graph_node->pose.translation = cubic_spline_interpolate(
            interpolation_value, p0.second.motion.translation,
            delta * no_scale_m0.second.motion.translation,
            p1.second.motion.translation,
            delta * no_scale_m1.second.motion.translation);

      } break;
      case channel::path::scale: {
        chan.target_graph_node->pose.scale = cubic_spline_interpolate(
            interpolation_value, p0.second.motion.scale,
            delta * no_scale_m0.second.motion.scale, p1.second.motion.scale,
            delta * no_scale_m1.second.motion.scale);
      } break;
      case channel::path::rotation: {
        chan.target_graph_node->pose.rotation = cubic_spline_interpolate(
            interpolation_value, p0.second.motion.rotation,
            delta * no_scale_m0.second.motion.rotation,
            p1.second.motion.rotation,
            delta * no_scale_m1.second.motion.rotation);
      } break;
      default:
        break;
    }
  }

  void apply_channel_target_for_interpolation_value(
      float interpolation_value, sampler::interpolation mode, int lower_frame,
      int upper_frame, float lower_time, float upper_time,
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
        apply_cubic_spline(interpolation_value, lower_frame, upper_frame,
                           lower_time, upper_time, chan);
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
      // assert(current_time >= sampler.min_v && current_time <= sampler.max_v);

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

        apply_channel_target_for_interpolation_value(
            interpolation_value, sampler.mode, lower_frame, upper_frame,
            lower_time, upper_time, channel);
      }
    }
  }
};
