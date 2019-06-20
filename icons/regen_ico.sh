#!/bin/bash

#generate all png sizes
convert gltf-insight.png -resize 16x16 		gltf-insight-16.png
convert gltf-insight.png -resize 32x32 		gltf-insight-32.png
convert gltf-insight.png -resize 48x48 		gltf-insight-48.png
convert gltf-insight.png -resize 64x64 		gltf-insight-64.png
convert gltf-insight.png -resize 96x96 		gltf-insight-96.png
convert gltf-insight.png -resize 128x128 	gltf-insight-128.png
convert gltf-insight.png -resize 256x256 	gltf-insight-256.png

#convert to windwos ico
convert gltf-insight-16.png gltf-insight-32.png gltf-insight-48.png gltf-insight-64.png gltf-insight-96.png gltf-insight-128.png gltf-insight-256.png gltf-insight.ico

#generate "filename.png.inc.hh" files with hexdump
for i in $(ls *.png)
do
	xxd -i $i > $i.inc.hh
done

