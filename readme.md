# BLOSPRAY - OSPRay as a Blender render engine

...

Note that this project is developed for Blender 2.8x, as that
will become the new stable version in a couple of months.
Blender 2.7x is not supported at the moment and probably won't be,
as the Python API isn't fully compatible.

## Usage

BLOSPRAY currently isn't a real Blender add-on. It is best used by executing it as a script during startup, followed by switching the renderer to `OSPRAY`. The following command-line accomplishes both:

```
$ blender -P blospray.py -E OSPRAY
```

XXX com

## Features

* Render server



* Custom properties

  We use Blender's [custom properties](https://docs.blender.org/manual/en/dev/data_system/custom_properties.html)
  to pass information to OSPRay. In future we expect to add new
  UI panels to Blender as a nicer way to pass this information, but custom properties serve their purpose well for now. They can even be animated, with certain limitations.

* Plugins

  There is a rudimentary plugin system that can be used to set up
  custom scene elements in OSPRay directly. This is especially useful when working with large scientific datasets for which it is infeasible or unattractive to load into Blender. Instead, you can use a proxy object, such as a cube mesh, and attach a plugin to it. During rendering BLOSPRAY will call the plugin to load the actual scene data associated with the proxy. The original use case for plugins (and even BLOSPRAY itself) was
  to make it easy to add a volume to a Blender scene, as Blender
  itself doesn't have good volume rendering support.

## Supported elements

* Basic mesh geometry

* Volumes

* Lights

* Basic render settings


## Known limitations and bugs

* There are no BLOSPRAY-specific UI panels yet, while most of the regular panels that offer renderer-specific settings (e.g. for Cycles) are disabled. Most of the settings that you would normally change in a UI panel are currently
managed through custom properties.

* Hierarchies of transformations (i.e. when using parenting) are not
exported correctly yet, causing incorrect positions of objects. Top-level objects work correctly, though.

* Modifiers on objects are not handled yet

* Only final render (i.e. F12) is supported, preview rendering appears to be technically feasible, but is not implemented yet.

* Error handling isn't very good yet, causing a lockup in the Blender script in case the BLOSPRAY server does something wrong (like crash ;-))

* BLOSPRAY is only being developed on Linux at the moment, on other platforms it might only work after some tweaks

* At some point BLOSPRAY needs to be turned in a real add-on

* All rendering is done on the CPU, because, well ... it's OSPRay ;-)

* Command-line (batch) rendering isn't supported in a nice way yet, as the BLOSPRAY server needs to be managed manually.

## Dependencies

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
