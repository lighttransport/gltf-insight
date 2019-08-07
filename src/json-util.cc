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
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#include "fmt/core.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include "json-util.hh"

using nlohmann::json;

namespace gltf_insight {

// req_len = -1 => allow arbitrary array length
bool DecodeNumberArray(const json& j, int req_len,
                              std::vector<float>* values) {
  if (values == nullptr) {
    return false;
  }

  values->clear();

  if (!j.is_array()) {
    return false;
  }

  if (req_len > 0) {
    if (req_len != int(j.size())) {
      fmt::print("Array length must be {} but got {}.\n", req_len, j.size());
      return false;
    }
  }

  for (auto& elem : j) {
    if (!elem.is_number()) {
      fmt::print("non-number value in an array found.\n");
      return false;
    }

    float value = float(elem.get<double>());

    values->push_back(value);
  }

  return true;
}

}  // namespace gltf_insight
