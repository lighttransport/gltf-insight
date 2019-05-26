#include "animation.hh"

void animation::set_playing_state(bool state) { playing = state; }

void animation::add_time(float delta) {
  // TODO has a setting to choose if we are looping or not
  if (current_time < min_time) current_time = min_time;
  current_time += delta;
  if (current_time > max_time)
    current_time = min_time + current_time - max_time;

  if (playing) {
    apply_pose();
  }
}

void animation::set_time(float time) {
  current_time = glm::clamp(time, min_time, max_time);
}

void animation::compute_time_boundaries() {
  // initialize min and max to the first value in the sampler
  min_time = samplers[0].min_v;
  max_time = samplers[0].max_v;

  // Compare all of them
  for (const auto& sampler : samplers) {
    min_time = std::min(min_time, sampler.min_v);
    max_time = std::max(max_time, sampler.max_v);
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

animation::animation()
    : current_time(0), min_time(0), max_time(0), playing(false), name() {}

void animation::apply_pose() {
  for (auto& channel : channels) {
    const auto& sampler = samplers[channel.sampler_index];

    // TODO probably a special case when animation has only *one* keyframe :
    // see https://github.com/KhronosGroup/glTF/issues/1597

    // Storage for the lower and upper keyframes
    std::array<std::pair<int, float>, 2> keyframe_interval;
    bool found = false;

    // Search the 2 keyframes that we need to interpolate between
    for (size_t frame = 0; frame < sampler.keyframes.size() - 1; frame++) {
      keyframe_interval[0] = sampler.keyframes[frame];
      keyframe_interval[1] = sampler.keyframes[frame + 1];

      if (keyframe_interval[0].second <= current_time &&
          keyframe_interval[1].second >= current_time) {
        found = true;
        break;
      }
    }

    // If found, compute the interpolation value
    if (found) {
      const int lower_frame = keyframe_interval[0].first;
      const int upper_frame = keyframe_interval[1].first;
      const float lower_time = keyframe_interval[0].second;
      const float upper_time = keyframe_interval[1].second;

      // current_time is a value between [lower_time; upper_time]
      // we want to change that to be between [0.f; 1.f]
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

void animation::apply_channel_target_for_interpolation_value(
    float interpolation_value, sampler::interpolation mode, int lower_frame,
    int upper_frame, float lower_time, float upper_time, const channel& chan) {
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

void animation::apply_step(const channel& chan, int lower_keyframe) {
  switch (chan.mode) {
    case channel::path::weight: {
      const auto nb_weights = chan.target_graph_node->pose.blend_weights.size();
      for (int w = 0; w < nb_weights; ++w)
        chan.target_graph_node->pose.blend_weights[w] =
            chan.keyframes[lower_keyframe * nb_weights + w]
                .second.motion.weight;
    } break;
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

void animation::apply_linear(const channel& chan, int lower_keyframe,
                             int upper_keyframe, float mix) {
  switch (chan.mode) {
    case channel::path::weight: {
      const auto nb_weights = chan.target_graph_node->pose.blend_weights.size();
      for (int w = 0; w < nb_weights; ++w) {
        chan.target_graph_node->pose.blend_weights[w] =
            glm::mix(chan.keyframes[lower_keyframe * nb_weights + w]
                         .second.motion.weight,
                     chan.keyframes[upper_keyframe * nb_weights + w]
                         .second.motion.weight,
                     mix);
      }
    } break;
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

inline int input_tangent(int frame) { return 3 * frame + 0; }

inline int output_tangent(int frame) { return 3 * frame + 2; }

inline int value(int frame) { return 3 * frame + 1; }

void animation::apply_cubic_spline(float interpolation_value, int lower_frame,
                                   int upper_frame, float lower_time,
                                   float upper_time, const channel& chan) {
  /*
   * When the sampler is set to cubic spline interpolation, each keyframe in
   * the channel is actually composed of 3 elements :
   *
   *  - An Input Tangent
   *  - The value at the keyframe
   *  - An output Tangent
   *
   *  The "channel" array is thus 3 times bigger than the number of keyframes
   * defined in the sampler.
   *
   *  To make the retrial code a bit more explicit, the 3 functions declared
   * above return the index of the element in the array that correspond to the
   * name of the function, given the keyframe index
   *
   *  The input and output tangent needs to be scaled by the keyframe duration
   * (upper_time - lower_time), the define the level of "cubic smoothing"
   * around the time point.
   *
   *  To interpolate the value at "current time" we need the point
   * corresponding to the lower and upper frame (p0 and p1) and we need the
   * output tangent of the lower frame, and the input tangent of the upper
   * frame.
   *
   *  The scaled values are called m0 and m1 in the glTF specification
   * (Appendix C).
   *
   */

  const auto frame_delta = upper_time - lower_time;

  // Retrieve data
  const auto p0 = chan.keyframes[value(lower_frame)];
  const auto unscaled_m0 = chan.keyframes[output_tangent(lower_frame)];
  const auto unscaled_m1 = chan.keyframes[input_tangent(upper_frame)];
  const auto p1 = chan.keyframes[value(upper_frame)];
  // m0 and m1 need to be scaled by this value (delta between the 2 keyframes
  // being interpolated)

  switch (chan.mode) {
    case channel::path::weight: {
      const auto nb_weights = chan.target_graph_node->pose.blend_weights.size();
      for (int w = 0; w < nb_weights; ++w) {
        chan.target_graph_node->pose.blend_weights[w] =
            cubic_spline_interpolate(
                interpolation_value, p0.second.motion.weight,
                frame_delta * unscaled_m0.second.motion.weight,
                p1.second.motion.weight,
                frame_delta * unscaled_m1.second.motion.weight);
      }
    } break;
    case channel::path::translation: {
      chan.target_graph_node->pose.translation = cubic_spline_interpolate(
          interpolation_value, p0.second.motion.translation,
          frame_delta * unscaled_m0.second.motion.translation,
          p1.second.motion.translation,
          frame_delta * unscaled_m1.second.motion.translation);

    } break;
    case channel::path::scale: {
      chan.target_graph_node->pose.scale = cubic_spline_interpolate(
          interpolation_value, p0.second.motion.scale,
          frame_delta * unscaled_m0.second.motion.scale, p1.second.motion.scale,
          frame_delta * unscaled_m1.second.motion.scale);
    } break;
    case channel::path::rotation: {
      chan.target_graph_node->pose.rotation = cubic_spline_interpolate(
          interpolation_value, p0.second.motion.rotation,
          frame_delta * unscaled_m0.second.motion.rotation,
          p1.second.motion.rotation,
          frame_delta * unscaled_m1.second.motion.rotation);
    } break;
    default:
      break;
  }
}

animation::sampler::sampler()
    : mode(interpolation::not_assigned),
      min_v(0),
      max_v(std::numeric_limits<float>::max()) {}

animation::channel::keyframe_content::keyframe_content() {
  std::memset(this, 0, sizeof(keyframe_content));
}

animation::channel::channel()
    : sampler_index(-1),
      target_node(-1),
      target_graph_node(nullptr),
      mode(path::not_assigned) {}
