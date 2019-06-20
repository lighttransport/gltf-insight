#!/bin/bash

rm -rf build_appimage
mkdir build_appimage
cd build_appimage
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make -j 2
make install DESTDIR=AppDir
ls AppDir
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy-x86_64.AppImage
./linuxdeploy-x86_64.AppImage --appdir AppDir --output appimage -d ../icons/gltf-insight.desktop -i ../icons/gltf-insight.svg


