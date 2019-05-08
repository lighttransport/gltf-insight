# Simple C++11 based glTF 2.0 data insight tool [![Build Status](https://travis-ci.org/lighttransport/gltf-insight.svg?branch=devel)](https://travis-ci.org/lighttransport/gltf-insight)

gltf-insight is a C++11 based data insight tool for glTF 2.0 model.
gltf-insight uses TinyGLTF for load/save glTF model and ImGUI for GUI.

## Requirements

* CMake
* C++11 compiler
* OpenGL 2.0(just for GUI stuff)

## Supported platforms

* [x] Windows
* [x] Linux
* [x] macOS

## Setup

```bash
git submodule init;
git submodule update;
```
## Build

In the genral case, just run CMake to generate a build system for your own environemnet

### Windows + Visual Studio 2017

Please run `vcsetup.bat` to generate project files.


### Linux and macOS

```
$ mkdir build
$ cd buid
$ cmake ..
$ make
```

## TODO

* bone display
* keyframe display
* glTF compliant keyframe interpolation code
* Animation curve display : https://github.com/ocornut/imgui/issues/786
* load morph target
* morph target blending code

## License

MIT license

### Third party licenses

* TinyGLTF : MIT license
* json.hpp : MIT license
* stb_image libraries : Public domain
* glad : MIT license
* glfw : zlib/png license
* glm : MIT license
* ImGUI : MIT license
* cxxopts : MIT license
