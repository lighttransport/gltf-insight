# glTF 2.0 data insight tool [![Build Status](https://travis-ci.org/lighttransport/gltf-insight.svg?branch=devel)](https://travis-ci.org/lighttransport/gltf-insight) [![Build status](https://ci.appveyor.com/api/projects/status/pb5f6g3qwxrrnxga/branch/devel?svg=true)](https://ci.appveyor.com/project/Ybalrid/gltf-insight/branch/devel)

<img align="left" width="190" height="190" src="icons/gltf-insight.png">

`gltf-insight` is a C++11 based data insight tool for glTF 2.0 model.

For example, you can display and tweak animation prameters this tool(since glTF 2.0 spec does not allow ASCII serialization for animation parameters)

`gltf-insight` also could be another reference viewer for glTF animation and PBR shading.

`gltf-insight` uses [TinyGLTF](https://github.com/syoyo/tinygltf) for load/save glTF model and ImGUI for GUI.



## Status

Work-in-progress.

## Requirements

* CMake
* C++11 compiler
* OpenGL 3.0

## Supported platforms

* [x] Windows
* [x] Linux
* [x] macOS

## Features

* glTF animation evaluation
  * [x] morph - Software blending between any number of morph targets
  * [x] skin - Hardware blending with max 4 joint attributes per vertex
  * [ ] skin - Software blending between an arbitrary number of joints per vertex
* Data visualization
  * [x] Skin weight

## Setup

```bash
git submodule update --init --recursive
```
## Build

In the genral case, just run CMake to generate a build system for your own environemnet

### Windows + Visual Studio 2017

Please run `vcsetup.bat` to generate project files.

#### From a git for Windows(mintty)

You can use `cmd //c` to call batch file from a mintty terminal.

```cmd
cmd //c vcsetup.bat
```

### Linux and macOS

```bash
mkdir build
cd buid
cmake ..
make
```

On Ubuntu, you may need to install a few dependencies : 

```bash
sudo apt install git cmake mesa-common-dev libgl1-mesa-dev libxrandr-dev libxcb-xinerama0 libxinerama-dev libxcursor-dev libxi-dev
```

## TODO

* [ ] PBR shading(in CPU)
* [ ] Animation curve display : https://github.com/ocornut/imgui/issues/786
* [ ] Edit animation parameters in GUI
* [ ] Better GUI for animations.
* [ ] CPU skinning.
* [ ] Draco compressed mesh support. https://github.com/google/draco
* [ ] basis_universal texture compression support. https://github.com/binomialLLC/basis_universal

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
* ImGuiFileDialog : MIT license
* dirent for Win32 : MIT license
* cxxopts : MIT license
* ionicons icon helper header : zlib licence
* ionic framework icon font : MIT
* Roboto icons : Apache 2

*“glTF and the glTF logo are trademarks of the Khronos Group Inc.”*
