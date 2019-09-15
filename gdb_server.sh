#!/bin/sh
LD_LIBRARY_PATH=$HOME/software/ospray-devel-git/lib64/:$LD_LIBRARY_PATH gdb ./bin/blserver $*
