### Changes in version 0.2 (unreleased)

* Added separate controls for render and viewport samples
* Added framebuffer "Update rate" for final renders so 
  the rate at which the framebuffer is updated can be controlled
* Rudimentary animation support:
  - Handle animated camera when final rendering animation sequences
  - Added way to use the current frame number in plugin property 
    values, so data that is frame-dependent can be specified. This 
    uses a simple template syntax, `{{<expression>}}`. 
    Currently, `frame` is the only available value to use in the 
    expression, but can already be used for something like 
    `/data/step{{'%04d' % frame}}.bin`.
    
Plugins:

* Added `geometry_vtk_streamlines.cpp` as an example of what can be
  done using VTK data loading, together with the OSPRay streamline
  geometry.

### Changes in version 0.1

Initial release. 

Consider this somewhere between alpha and beta quality:
some things work and are practically usable, some things are broken
and some things need further refactoring which will impact the plugin API
and/or Blender scene setup. No verification has also been done at this
point that the renders produced match the underlying data in the correct
way (although at first glance they do match).

Features (and remarks):

* Scene elements
    - Polygonal geometry, i.e. Blender meshes
    - Subdivision surfaces: these are handled by Blender's dependency graph mechanism,
      but this triangulates the geometry instead of passing the subdivision control
      cage to OSPRay (which also has subdiv support).
    - Currently no specific support for other non-polygonal objects (e.g. text, curves, ...),
      if it works it's due to the dependency graph mechanism
    - Object transformations and parenting
    - Instancing: for large numbers of instances there currently is 
      quite a high overhead in terms of scene setup and memory usage    
    - Point, sun, spot and area lights. No support for OSPRay's HDRI light yet
    - Vertex colors on Blender meshes
    - Node-based material editing for a subset of OSPRay materials,
      using nodes from the `OSPRay` sub-menu. Note that connecting
      nodes to input ports, like a color, will not work as expected 
      when a connection is made (and there is no way to hide the sockets to signal this).
      Also, although most of the Cycles nodes are still available in the
      shader editor almost NONE of those nodes will work.    
* Rendering
    - Supports OSPRay's SciVis and Path Tracer renderers, and most of
      their settings
    - Final render (F12), including canceling
    - Interactive preview render works with respect to viewpoint changes, 
      but most other scene changes will NOT update in the rendered view. 
      Switching out and back into rendered view should sync to the updated scene
    - The view will match in orthographic and "free" view mode, but it is
      incorrect when viewing through a camera
    - Border render (i.e. render only part of an image) currently only works 
      for final render, not for interactive render      
    - Simultaneous rendering modes in Blender are not supported. E.g. multiple
      3D views each in interactive rendering mode will not work as the render
      server does not support this
    - Material/light/... preview rendering does not work
* Camera
    - Perspective and orthographic cameras, plus OSPRay's panoramic camera.
      The latter is similar to Cycles' equirectangular camera, but without 
      any parameters to tweak
    - Depth of field
* Blender UI
    - Integration within UI, mostly panels for editing properties and such, 
      is not very advanced. Some properties are currently only settable using Blender
      [custom properties](https://docs.blender.org/manual/en/latest/data_system/custom_properties.html) 
      on objects and meshes.
* Rudimentary support for BLOSPRAY-specific volume, geometry and scene plugins      
* Rudimentary transfer function editing for volume data (by mis-using the ColorRamp node)
  
Notes on current (2.0.x alpha) OSPRay, not BLOSPRAY, limitations and/or issues:

* When using the scivis renderer a transformed volume will not render
  correctly 
* Scaling a volume appears to influence the density of the samples (needs
  verification to be an OSPRay issue)

  