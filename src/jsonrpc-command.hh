#ifndef GLTF_INSIGHT_JSONRPC_COMMAND_HH_
#define GLTF_INSIGHT_JSONRPC_COMMAND_HH_

namespace gltf_insight {

struct Xform {
  std::array<float, 3> translation = {0.0f, 0.0f, 0.0f};
  std::array<float, 4> rotation = {0.0f, 0.0f, 0.0f, 1.0f};  // quat
  std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};
};

///
/// TODO(LTE): Create a class hierarchy once we need to handle more commands and
/// parameters.
///
struct Command {
 public:
  enum Type {
    JOINT_TRANSFORM,  // overwrite transform
    ADDITIVE_JOINT_TRANSFORM, // add transform to existing pose
    MORPH_WEIGHT,
    ANIMATION_CLIP,
    TIMELINE_CURRENT_FRAME,
    TIMELINE_CURRENT_TIME,
    TIMELINE_SET_FPS,
  };

  Type type;

  // for JOINT_TRANSFORM and JOINT_TRANSFORM_ADDITIVE
  std::pair<int, Xform> joint_transform;  // <joint_id, xform>

  // for MORPH_WEIGHT
  std::pair<int, float> morph_weight;  // <target_id, weight>

  // for ANIMATION_CLIP
  float animation_clip_time = 0.0f;

  // for TIMELINE_CURRENT_FRAME
  int current_frame = 0;

  // for TIMELINE_CURRENT_TIME
  float current_time = 0.0f;

  // for TIMELINE_SET_FPS
  int fps = 60;
};

}  // namespace gltf_insight

#endif  // GLTF_INSIGHT_JSONRPC_COMMAND_HH_
