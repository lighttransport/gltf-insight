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

#include "audio.hh"

#include <fstream>
#include <iostream>

// $gltf-insight/deps

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif

#define DR_FLAC_IMPLEMENTATION
#include "miniaudio/extras/dr_flac.h" /* Enables FLAC decoding. */
#define DR_MP3_IMPLEMENTATION
#include "miniaudio/extras/dr_mp3.h" /* Enables MP3 decoding. */
#define DR_WAV_IMPLEMENTATION
#include "miniaudio/extras/dr_wav.h" /* Enables WAV decoding. */

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio/miniaudio.h"

#ifdef __clang__
#pragma clang diagnostic pop
#endif

namespace gltf_insight {

static void data_callback(ma_device* pDevice, void* pOutput, const void* pInput,
                          ma_uint32 frameCount) {
  ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
  if (pDecoder == NULL) {
    return;
  }

  ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount);

  (void)pInput;
}

Audio::~Audio() {
  if (_initialized) {
    ma_device_uninit(&_device);
    ma_decoder_uninit(&_decoder);
  }
}

bool Audio::load_from_file(const std::string& filename, std::string* err) {
  {
    std::ifstream f(filename, std::ifstream::binary);
    if (!f) {
      if (err) {
        (*err) += "File open error : " + filename + "\n";
      }
      return false;
    }

    f.seekg(0, f.end);
    size_t sz = static_cast<size_t>(f.tellg());
    f.seekg(0, f.beg);

    if (int(sz) < 16) {
      if (err) {
        (*err) += "Invalid file size : " + filename +
                  " (does the path point to a directory?)";
      }
      return false;
    }

    _data.resize(sz);
    f.read(reinterpret_cast<char*>(_data.data()),
           static_cast<std::streamsize>(sz));
  }

  ma_result result;

  result = ma_decoder_init_memory(_data.data(), _data.size(),
                                  /* config */ nullptr, &_decoder);
  if (result != MA_SUCCESS) {
    if (err) {
      (*err) = "Failed to decode audio file : " + filename + "\n";
    }
    return false;
  }

  _device_config = ma_device_config_init(ma_device_type_playback);
  _device_config.playback.format = _decoder.outputFormat;
  _device_config.playback.channels = _decoder.outputChannels;
  _device_config.sampleRate = _decoder.outputSampleRate;
  _device_config.dataCallback = data_callback;
  _device_config.pUserData = &_decoder;

  if (ma_device_init(nullptr, &_device_config, &_device) != MA_SUCCESS) {
    if (err) {
      (*err) = "Failed to open playback device.\n";
    }
    ma_decoder_uninit(&_decoder);
    return false;
  }

  _uri = filename;
  _initialized = true;

  return true;
}

bool Audio::play() {
  if (!_initialized) {
    return false;
  }

  if (is_playing()) {
    return false;
  }

  // NOTE(LTE): It looks miniaudio goes to inifnitely playing state
  // after ma_device_start() was called.
  // To re-play audio(device), we have to call ma_device_stop().
  //
  // We may use stop_callback function to explicitly terminate the device
  // when it finishes playing audio data.
  //
  if (ma_device_start(&_device) != MA_SUCCESS) {
    std::cerr << "Failed to start playback device.\n";

    ma_device_uninit(&_device);
    ma_decoder_uninit(&_decoder);
    _initialized = false;
    return false;
  }

  std::cout << "Started audio.\n" << std::endl;

  return true;
}

bool Audio::pause() {
  if (!_initialized) {
    return false;
  }

  if (ma_device_stop(&_device) != MA_SUCCESS) {
    std::cerr << "Failed to stop playback device.\n";
    return false;
  }

  std::cout << "Audio paused.\n";
  return true;
}

bool Audio::stop() {
  if (!_initialized) {
    return false;
  }

  // It looks we need to re-create device and decoder to play audio again.
  ma_device_uninit(&_device);
  ma_decoder_uninit(&_decoder);

  ma_result result;
  result = ma_decoder_init_memory(_data.data(), _data.size(),
                                  /* config */ nullptr, &_decoder);
  if (result != MA_SUCCESS) {
    std::cerr << "Failed to decode audio data\n";
    _initialized = false;
    return false;
  }

  _device_config = ma_device_config_init(ma_device_type_playback);
  _device_config.playback.format = _decoder.outputFormat;
  _device_config.playback.channels = _decoder.outputChannels;
  _device_config.sampleRate = _decoder.outputSampleRate;
  _device_config.dataCallback = data_callback;
  _device_config.pUserData = &_decoder;

  if (ma_device_init(nullptr, &_device_config, &_device) != MA_SUCCESS) {
    std::cerr << "Failed to open playback device.\n";
    // ma_decoder_uninit(&_decoder);
    return false;
  }

  std::cout << "Stop audio.\n";
  return true;
}

bool Audio::is_playing() {
  if (!_initialized) {
    return false;
  }

  ma_bool32 ret = ma_device_is_started(&_device);
  if (ret == MA_TRUE) {
    return true;
  }

  return false;
}

}  // namespace gltf_insight
