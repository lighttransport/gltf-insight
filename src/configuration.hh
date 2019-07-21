#pragma once

#include <glm/glm.hpp>

namespace gltf_insight {
struct configuration {
  static glm::vec4 highlight_color;
  static float vertex_highlight_size;
  static glm::vec4 joint_draw_color;
  static glm::vec4 joint_highlight_color;
  static glm::vec4 bone_draw_color;
  static glm::vec4 bone_highlight_color;
  static float joint_draw_size;
  static float bone_draw_size;
  static bool editor_configuration_open;
  static void show_editor_configuration_window();

  // TODO serialize/load configuration
};
}  // namespace gltf_insight
