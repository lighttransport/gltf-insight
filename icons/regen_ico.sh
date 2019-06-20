#!/bin/bash

convert gltf-insight.png -resize 16x16 16.png
convert gltf-insight.png -resize 32x32 32.png
convert gltf-insight.png -resize 48x48 48.png
convert gltf-insight.png -resize 64x64 64.png
convert gltf-insight.png -resize 96x96 96.png
convert gltf-insight.png -resize 128x128 128.png
convert gltf-insight.png -resize 256x256 256.png

convert 16.png 32.png 48.png 64.png 96.png 128.png 256.png gltf-insight.ico
