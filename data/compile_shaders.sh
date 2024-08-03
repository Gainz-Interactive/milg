#!/bin/bash

cd $(dirname $0)/shaders
for file in $(ls | grep -v .spv); do
    echo "Compiling $file"
    glslc $file -o $file.spv
done
