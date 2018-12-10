# BLOSPRAY - OSPRay as a Blender render engine

...

Note that this project is developed for Blender 2.8x, as that
will become the new stable version in a couple of months.
Blender 2.7x is not supported at the moment and probably won't be,
as the Python API isn't fully compatible with 2.8x.

## Usage

BLOSPRAY is as a Blender add-on, but is not being distributed separately.
Currently, the way to install it is to clone this repository and then
make a symlink to the `render_ospray` directory in the Blender addon directory:

```
$ cd ..../blender-2.8/2.80/scripts/addons
$ ln -sf <blospray-repo>/render_ospray render_ospray
```

## Features

* Plugins

  There is a rudimentary plugin system that can be used to set up
  custom scene elements in OSPRay directly. This is especially useful when working with large scientific datasets for which it is infeasible or unattractive to load into Blender. Instead, one can use a proxy object, such as a cube mesh, and attach a plugin to it. During rendering BLOSPRAY will call the plugin to load the actual scene data associated with the proxy. In this way Blender scene creation, such as camera animation or lighting, can be done as usual as the proxy object shows the bounding box of the data and can even be transformed.

  The original use case for plugins (and even BLOSPRAY itself) was
  to make it easy to add a rendered volume in a Blender scene, as Blender
  itself doesn't have good volume rendering support. But plugins can be used for other types of scene contact as well, like polygonal geometry.

* Render server

  BLOSPRAY consists of two parts:

  1. a Python script (`blospray.py`) that implements the Blender render engine. It handles scene export, showing the rendered result, etc.
  2. a render server (`ospray_render_server`) that receives the scene from Blender, calls OSPRay routines to do the actual rendering and sends back the image result.

  The original reason for this two-part setup is that there currently is no Python API for OSPRay, so direct integration in Blender is not straightforward. Neither is there a command-line OSPRay utility that takes as input a scene description in some format and outputs a rendered image.

  Plus, the client-server setup also has some advantages:

  - The separate render server can be run on a remote system, for example an HPC system that holds a large scientific dataset to be rendered. This offloads most of the compute-intensive rendering workload and necessity to hold data to be rendered locally away from the system running Blender.
  - It should be feasible to use OSPRay's [Parallel Rendering with MPI](http://www.ospray.org/documentation.html#parallel-rendering-with-mpi) mode, by providing a variant of the render server as an MPI program.
  - BLOSPRAY development becomes slightly easier as both Blender and the render server can be independently restarted in case of crashes or bugs.

  Of course, this client-server setup does introduce some overhead, in terms of network latency and data (de)serialization. But in practice this overhead is small compared to actual render times.

## Supported elements

* Basic mesh geometry

* Volumes

* Lights

* Basic render settings


## Known limitations and bugs

* The addon provides some UI panels to set OSPRay specific settings, but in other case we use Blender's [custom properties](https://docs.blender.org/manual/en/dev/data_system/custom_properties.html)
  to pass information to OSPRay. These can even be animated, with certain limitations, but are not a long-term solution. Note also that builtin UI panels are disabled when the render engine
  is set to OSPRay as those panels can't directly be used with OSPRay (e.g. they contain Cycles-specific settings).

* Scene management on the render server is non-existent. I.e. memory usage increases after each render.  

* Caching of scene data on the server, especially for large data loaded by plugins, is planned to be implemented, but isn't there yet. I.e. currently all scene data is re-sent when rendering a new image.

* Only a single (hard-coded) transfer function for volume rendering is supported

* In OSPray structured grid volumes currently cannot be transformed with an arbitrary affine transformation (see [this issue](https://github.com/ospray/ospray/issues/159)). We work around this limitation by converting a structured grid to an unstructured one, whose vertices *can* be transformed. Of course, this increases memory usage and decreases render performance, but makes it possible to handle volumes in the same general was as other Blender scene objects. XXX backup

* Hierarchies of transformations (i.e. when using parenting) are not exported correctly yet, causing incorrect positions of objects. Top-level objects work correctly, though.

* Modifiers on objects are not handled yet

* Only final renders (i.e. using the F12 key) are supported. Preview rendering is technically feasible, but is not implemented yet.

* Error handling isn't very good yet, causing a lockup in the Blender script in case the BLOSPRAY server does something wrong (like crash ;-))

* BLOSPRAY is only being developed on Linux at the moment, on other platforms it might only work after some tweaks

* All rendering is done on the CPU, because, well ... it's OSPRay ;-)

* Command-line (batch) rendering isn't supported in a nice way yet, as the lifetime of the BLOSPRAY server needs to be managed manually.


## Dependencies

* Numpy

* Google protobuf (C/C++ libraries needed during build, Python modules at runtime)

  To make Blender find the necessary protobuf packages add symlinks to
  `google` and `six.py` in Blender's python library dir:

  ```
  $ cd ~/blender-2.80-...../2.80/python/lib/python3.7
  $ ln -sf /usr/lib/python3.7/site-packages/six.py six.py
  $ ln -sf /usr/lib/python3.7/site-packages/google google
  ```

* Uses https://github.com/nlohmann/json/blob/develop/single_include/nlohmann/json.hpp
  (included in the sources)
