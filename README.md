# Simple C++11 based glTF 2.0 data insight tool

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

git submodule updat init:
## Build

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

* keyframe display
* Animation curve display : https://github.com/ocornut/imgui/issues/786

## License

MIT license

### Third party licenses

* TinyGLTF : MIT license
* json.hpp : MIT license
* glad : MIT license
* glfw : zlib/png licens
* ImGUI : MIT license
* cxxopts : MIT license
