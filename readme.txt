Needs protobuf during build
Needs python protobuf library during runtime

need to add symlinks to google and six python packages in blender's python lib dir:

$ cd ~/blender-2.80-...../2.80/python/lib/python3.7
$ ln -sf /usr/lib/python3.7/site-packages/six.py six.py
$ ln -sf /usr/lib/python3.7/site-packages/google google

uses https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp
