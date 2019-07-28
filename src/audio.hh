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
#ifndef GLTF_INSIGHT_AUDIO_HH_
#define GLTF_INSIGHT_AUDIO_HH_

// audio playback for (spatialized) sound support in glTF
//
// https://github.com/KhronosGroup/glTF/issues/1582
// https://github.com/KhronosGroup/glTF/pull/1400

// Currently we only support playback audio.
// TODO(LTE): mixing, spatialized sound, etc.

#include <string>
#include <vector>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

// $gltf_insight/deps
#include "miniaudio/miniaudio.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace gltf_insight {

class Audio {
 public:
  Audio() {}
  ~Audio();

  // Underlying miniaudio structs are non-copyable, so make Audio non-copyable
  // also.
  Audio(const Audio&) = delete;
  Audio& operator=(const Audio&) = delete;

  ///
  /// Loads audio from a file.
  ///
  /// @param[in] audio_file Audio filename(wav, mp3 or flac)
  /// @param[out] err Error message(if error happens).
  /// @return true upon success.
  ///
  bool load_from_file(const std::string& audio_file, std::string* err);

#if 0
  ///
  /// Loads audio from a memory.
  ///
  /// @param[in] binary binary data containing audio.
  /// @param[in] num_bytes The number of bytes.
  /// @param[out] err Error message(if error happens).
  /// @return true upon success.
  ///
  bool load_from_buffer(const uint8_t *binary, const size_t num_bytes, std::string *err);
#endif

  ///
  /// Simple playback audio(no synching. playback from a beginning of audio).
  ///
  bool play();

  const std::string& uri() { return _uri; }

  ///
  /// Pause playback.
  ///
  bool pause();

  ///
  /// Stop playback.
  /// Must be callbed before calling `play` if you want to re-play audio.
  ///
  bool stop();

  bool is_playing();

 private:
  std::string _uri;
  std::vector<uint8_t> _data;  // audio data

  bool _initialized = false;
  bool _playing = false;

  ma_decoder _decoder;
  ma_device_config _device_config;
  ma_device _device;
};

}  // namespace gltf_insight

#endif  // GLTF_INSIGHT_AUDIO_HH_
