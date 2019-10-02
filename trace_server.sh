#!/bin/sh
export FAKER_DUMP_ARRAYS=1
export FAKER_ABORT_ON_OSPRAY_ERROR=1
export LD_PRELOAD=/usr/lib/libasan.so:./bin/libfaker.so 

./bin/blserver $*
