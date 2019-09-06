#!/bin/sh
LD_LIBRARY_PATH=$HOME/software/ospray-devel-git/lib64/:$LD_LIBRARY_PATH ./bin/ospray_render_server $*
