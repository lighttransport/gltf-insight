#!/bin/bash

rm -rf build

cmake -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_ADDRESS=On -Bbuild -H.
