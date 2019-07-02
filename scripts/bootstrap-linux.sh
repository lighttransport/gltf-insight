#!/bin/bash

rm -rf build
mkdir build

cmake -Bbuild -H. -DGLTF_INSIGHT_USE_CCACHE=On -DGLTF_INSIGHT_USE_NATIVEFILEDIALOG=On
