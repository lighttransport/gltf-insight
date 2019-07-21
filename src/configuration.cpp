#include "configuration.hh"

#include <imgui.h>

#include <glm/gtc/type_ptr.hpp>

using namespace gltf_insight;

glm::vec4 configuration::highlight_color = glm::vec4(1, 0.5, 0, 1);
float configuration::vertex_highlight_size = 5;
glm::vec4 configuration::bone_draw_color = glm::vec4(0.f, .5f, .5f, 1.f);
glm::vec4 configuration::joint_draw_color = glm::vec4(1, 0, 0, 1);
glm::vec4 configuration::bone_highlight_color = glm::vec4(0.5, 0, 0.5, 1);
glm::vec4 configuration::joint_highlight_color = glm::vec4(0, 1, 0, 1);
float configuration::bone_draw_size = 3;
float configuration::joint_draw_size = 3;
bool configuration::editor_configuration_open = false;

void configuration::show_editor_configuration_window() {
  static const ImVec4 yellow(1, 1, 0, 1);
  if (!editor_configuration_open) return;
  if (ImGui::Begin("Editor Configuration", &editor_configuration_open)) {
    ImGui::TextColored(yellow, "General:");
    ImGui::ColorEdit3("Highlight color", glm::value_ptr(highlight_color));
    ImGui::SliderFloat("Vertex highlight size", &vertex_highlight_size, 1, 10);
    ImGui::TextColored(yellow, "Skeleton display:");
    ImGui::ColorEdit3("Bone", glm::value_ptr(bone_draw_color));
    ImGui::ColorEdit3("Bone (selected)", glm::value_ptr(bone_highlight_color));
    ImGui::SliderFloat("Bone size", &bone_draw_size, 1, 10);
    ImGui::ColorEdit3("Joint", glm::value_ptr(joint_draw_color));
    ImGui::ColorEdit3("Joint (selected)",
                      glm::value_ptr(joint_highlight_color));
    ImGui::SliderFloat("Joint size", &joint_draw_size, 1, 10);
  }
  ImGui::End();
}
