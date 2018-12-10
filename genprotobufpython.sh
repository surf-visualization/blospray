#!/bin/sh

protoc --python_out=./render_ospray core/messages.proto
