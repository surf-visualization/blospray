Needs protobuf during build
Needs python protobuf library during runtime

need to add symlinks to google and six python packages in blender's python lib dir:

$ ln -sf /usr/lib/python3.7/site-packages/six.py /home/paulm/software/blender-2.80-71fd7e610a6-linux-glibc224-x86_64/2.80/python/lib/python3.7

$ ln -sf /usr/lib/python3.7/site-packages/google /home/paulm/software/blender-2.80-71fd7e610a6-linux-glibc224-x86_64/2.80/python/lib/python3.7


uses https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp
