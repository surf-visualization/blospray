### Changes in version 0.1

Initial release. Consider this somewhere between alpha and beta quality:
some things work and are practically usable, some things are broken
and some things need further refactoring which will impact the plugin API
and/or Blender scene setup. No verification has also been done at this
point that the renders produced match the underlying data in the correct
way (although at first glance they do match).

Features:

* Supports OSPRay's SciVis and Path Tracer renderers
* Scene export and rendering of polygonal geometry (i.e. Blender meshes)
    - Subdivision surfaces. These are handled by Blender's dependency graph mechanism,
      but this triangulates the geometry instead of passing the subdivision control
      cage to OSPRay (which also has subdiv support).
    - No support for othernon-polygonal objects (e.g. text, curves, ...)
* Point, sun, spot and area lights
    - No support for OSPRay's HDRI light yet
* Vertex colors on Blender meshes
* Final render
* Interactive preview render 
    - Works with respect to viewpoint changes, but most other scene changes 
      will NOT update in the rendered view. Switching out and back into 
      rendered view should sync to the updated scene
    - The view will match in orthographic and "free" view mode, but is
      incorrect when viewing through a camera
* Object transformations and parenting
* Instancing
    - For large numbers of instances there currently is quite a high overhead
      in terms of scene setup and memory usage
* Perspective and orthographic cameras, plus OSPRay's panoramic camera 
  (which is similar to Cycles' equirectangular camera, but without any parameters to tweak)
* Depth of field
* Border render (i.e. render only part of an image)
    - This currently only works for final render, not for interactive render
* Rudimentary support for volume, geometry and scene plugins
* Node-based material editing for a subset of OSPRay materials
    - Note that input ports for node values, like a color, will not 
      work as expected when a connection is made (and there is no way 
      to hide the sockets to signal this)
* Rudimentary transfer function editing for volume data (by mis-using the ColorRamp node)
* Integration within the Blender UI, mostly panels for editing properties and such, 
  is not very advanced. Some properties are currently only settable using Blender
  [custom properties](https://docs.blender.org/manual/en/latest/data_system/custom_properties.html) on objects and meshes.
* Only a single connection to the render server (see below) is handled 
  at a time
* Simulatenous rendering modes in Blender are not supported. E.g. multiple
  3D views each in interactive rendering mode will not work as the render
  server does not support this.
  
Notes on current (2.0.x alpha) OSPRay limitations:

* When using the scivis renderer a transformed volume will not render
  correctly 

  