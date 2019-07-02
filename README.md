# glTF 2.0 data insight tool [![Build Status](https://travis-ci.org/lighttransport/gltf-insight.svg?branch=devel)](https://travis-ci.org/lighttransport/gltf-insight) [![Build status](https://ci.appveyor.com/api/projects/status/pb5f6g3qwxrrnxga/branch/devel?svg=true)](https://ci.appveyor.com/project/Ybalrid/gltf-insight/branch/devel)

<img align="left" width="190" height="190" src="icons/gltf-insight.png">

`gltf-insight` is a C++11 based data insight tool for glTF 2.0 model.

For example, you can display and tweak animation prameters this tool(since glTF 2.0 spec does not allow ASCII serialization for animation parameters)

`gltf-insight` also could be another reference viewer for glTF animation and PBR shading.

`gltf-insight` uses [TinyGLTF](https://github.com/syoyo/tinygltf) for load/save glTF model and ImGUI for GUI.



## Status

Work-in-progress.

### glTF extension support:

general:
 - [ ] `KHR_draco_mesh_compression` Note: This is supported directly inside tinygltf

material:
 - [x] `KHR_materials_unlit`
 - [ ] `KHR_materials_pbrSpecularGlossiness` (**TODO** only suport fallback to metal_roughness ATM. Dedicated shader needed)
 - [ ] `KHR_texture_transform`


lighting:
 - [ ] `EXT_lights_image_based`


Latest coninious build are available for [**Linux** (AppImage)](https://github.com/lighttransport/gltf-insight/releases/tag/continuous) and [**Windows** (portable exe)](https://github.com/lighttransport/gltf-insight/releases/tag/continuous-appveyor). Both are 64bit

## Requirements

* CMake 3.5 or later
* C++11 compiler
  * clang: 3.9 or later
* OpenGL 3.3

## Supported platforms

* [x] Windows
* [x] Linux
  * [x] Ubuntu 16.04
  * [x] CentOS7
* [x] macOS

## Features

* Load and display glTF assets
  * [x] glTF files with external resources
  * [x] glTF files with embeded resources
  * [x] glb files (binary glTF with enclosed resources)
  * [ ] Any of the above with darco mesh compression **TODO**
  * [x] Partial support for VRM avatars (simply treated as a standard glTF binary)

* glTF animation evaluation
This program can play the animations defined in a glTF asset by the following mean
  * [x] morph - Software blending between any number of morph targets
  * [x] skin - Hardware blending with max **4 joints** attributes per vertex
  * [ ] skin - Software blending between an arbitrary number of joints per vertex
  * [ ] **TODO** animation editing features

* Data visualization
  * [x] Skinning weights

## Setup

Every source dependencies is either enclosed within the source code, or is pulled from submodules. These dependences also uses subomudles. You will need to get them recursively. This command should get you up and running:

```bash
git submodule update --init --recursive
```
## Build

In the genral case, just run CMake to generate a build system for your own environment. Some scripst are provided to facilitate the setup.

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

#### On Ubuntu

You may need to install a few dependencies :

```bash
sudo apt install git cmake mesa-common-dev libgl1-mesa-dev libxrandr-dev libxcb-xinerama0 libxinerama-dev libxcursor-dev libxi-dev
```

#### On CentOS7

You can use cmake3 package and You may need to install a few dependencies :

```bash
sudo yum install git cmake3 libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel
```

## TODO

* [ ] PBR shading(in CPU)
* [ ] Animation curve display : https://github.com/ocornut/imgui/issues/786
* [ ] Edit animation parameters in GUI
* [ ] Better GUI for animations.
* [x] CPU skinning.
* [ ] Draco compressed mesh support. https://github.com/google/draco
  * NOTE that Draco fails to compile with gcc4.8(CentOS7 default)
* [ ] basis_universal texture compression support. https://github.com/binomialLLC/basis_universal

## License

MIT license

### Third party licenses

gltf-insight is built upon the following open-source projects:
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
