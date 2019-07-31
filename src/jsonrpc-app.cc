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

// Include tinygltf's json.hpp
#include "json.hpp"

#include "fmt/core.h"

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

  std::function<void(const std::string&)> cb_f = [=](const std::string &msg) {
    bool ret =  this->jsonrpc_dispatch(msg);
    // TODO(LTE): Check return value.
    (void)ret;

  };

  std::thread th([&]{
    JSONRPC rpc;
    std::cout << "Listen...\n";
    bool ret = rpc.listen_blocking(cb_f, &_jsonrpc_exit_flag, _address, _port);
    std::cout << "Listen ret = " << ret << "\n";
  });

  _jsonrpc_thread = std::move(th);
  _jsonrpc_thread_running = true;

#endif

  return true;
}

bool app::jsonrpc_dispatch(const std::string &json_str)
{
  json j;

  try {
    j = json::parse(json_str);
  } catch (const std::exception &e) {
    std::cerr << "Invalid JSON message.\n";
    return false;
  }

  if (!j.is_object()) {
    std::cerr << "Invalid JSON message.\n";
    return false;
  }

  if (!j.count("method")) {
    std::cerr << "JSON message does not contain `method`.\n";
    return false;
  }

  json params = j["params"];

  if (params["method"].is_string()) {
    std::cerr << "`method` must be string.\n";
    return false;
  }

  std::string method = params["method"].get<std::string>();
  if (method.compare("update") != 0) {
    std::cerr << "`method` must be `update`, but got `" << method << "'\n";
    return false;
  }

  if (!j.count("params")) {
    std::cerr << "JSON message does not contain `params`.\n";
    return false;
  }


  if (params.count("morph_weights")) {
    json morph_weights = params["morph_weights"];

    std::cout << "morph_weights " << morph_weights << "\n";
  }

  return true;
}

} // namespace gltf_insight
