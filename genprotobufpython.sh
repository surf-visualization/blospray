#!/bin/sh

cd core

protoc --python_out=../render_ospray messages.proto

cd ..
