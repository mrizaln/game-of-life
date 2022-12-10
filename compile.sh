#!/usr/bin/env bash

opt=$2
if [[ -z $2 ]]; then
    opt="0"
fi
out="$3"
if [[ -z "$3" ]]; then
    out="a.out"
fi

rm "$out"

g++\
    -O$opt\
    -std=c++20\
    --include-directory=./include\
    -g "./include/glad/glad.c"\
    -g "$1"\
    -lglfw\
    -lGL\
    -lX11\
    -lpthread\
    -lXrandr\
    -lXi\
    -ldl\
    -o "$out"

echo "compile status: $?"
