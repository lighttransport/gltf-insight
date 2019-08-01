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
#include "jsonrpc-http.hh"

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// Include tinygltf's json.hpp
#include "json.hpp"

#include "fmt/core.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "jsonrpc-command.hh"

#include <iostream>
#include <vector>
#include <thread>

using nlohmann::json;

namespace gltf_insight {

bool app::spawn_http_listen()
{
#if !defined(GLTF_INSIGHT_WITH_JSONRPC)
  return false;
#else
  std::cout << "Start thread\n";

  _jsonrpc_exit_flag = false;

  std::function<void(const std::string&)> cb_f = [&](const std::string &msg) {
    bool ret =  this->jsonrpc_dispatch(msg);
    // TODO(LTE): Check return value.
    (void)ret;

  };

#if 0
  std::thread th([&]{
    JSONRPC rpc;
    std::cout << "Listen...\n";
    bool ret = rpc.listen_blocking(cb_f, &_jsonrpc_exit_flag, _address, _port);
    std::cout << "Listen ret = " << ret << "\n";
  });
#endif

  JSONRPC rpc;
  std::cout << "Listen...\n";
  bool ret = rpc.listen_blocking(cb_f, &_jsonrpc_exit_flag, _address, _port);
  std::cout << "Listen ret = " << ret << "\n";
  return true;

#endif

}

static bool DecodeMorphWeights(const json &j, std::vector<std::pair<int, float>> *params)
{
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

bool app::jsonrpc_dispatch(const std::string &json_str)
{
  json j;

  try {
    j = json::parse(json_str);
  } catch (const std::exception &e) {
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
    std::cerr << "JSONRPC version must be \"2.0\" but got \"" << version << "\"\n";
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

  }

  return true;
}

bool app::update_scene(const Command &command)
{

  if (command.type == Command::MORPH_WEIGHT) {
    std::pair<int, float> param = command.morph_weight;

    if (gltf_scene_tree.pose.blend_weights.size() > 0) {
      int idx = param.first;

      std::cout << idx << ", # of morphs = " << gltf_scene_tree.pose.blend_weights.size() << "\n";

      if ((idx >= 0) && (idx < int(gltf_scene_tree.pose.blend_weights.size()))) {
        float weight = std::min(1.0f, std::max(0.0f, param.second));

        std::cout << "Update " << idx << "th morph_weight with " << weight << "\n";
        gltf_scene_tree.pose.blend_weights[size_t(idx)] = weight;
      }
    }

  }

  return true;
}

} // namespace gltf_insight
