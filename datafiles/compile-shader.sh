#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: compile-shader.sh <name> <stage>"
    echo "  e.g. compile-shader.sh world frag"
    exit 1
fi

echo "glslc -fshader-stage=$2 -o $1${2:0:1}.spv $1.$2.glsl"
glslc -fshader-stage=$2 -o $1${2:0:1}.spv $1.$2.glsl