# Some notes and thoughts on using Blender for scientific visualization

## Introduction

After presenting BLOSPRAY at BCON19 and based on reactions and 
discussions that followed, also of the other projects presented
in the sciviz panel,  I wanted to put down some thoughts in this page.

There's quite a number of ongoing activities to make Blender more 
usable for scientific visualization:

- [BVtkNodes](https://github.com/simboden/BVtkNodes) (and the more
  active [fork](https://github.com/tkeskita/BVtkNodes]) for allowing
  VTK operations to be performed using node networks and use their
  output in a Blender scene. This was initially presented at BCON18.
- [Integration of COVISE in Blender](http://blender.it4i.cz/scientific-visualization/covise/),
  for data processing and analysis, as presented at BCON19.
- Not specifically related to scientific visualization, but interesting
  in an HPC context, is the [CyclesPhi](https://code.it4i.cz/blender/cyclesphi280) version
  of Cycles that allows distributed rendering of large scenes. It provides
  support for MPI and Xeon Phi (plus OpenMP parallelization).
- Finally, this project, BLOSPRAY, to make OSPRay-based rendering of scientific
  data available in Blender. It focuses on easy construction of scientific
  scenes in the OSPRay renderer, avoiding representing the full scenes
  in Blender itself. A second focus is providing the nice volume rendering
  that is present in OSPRay in Blender.
  
The reasons the above activities exist have to do with a difference in 
focus, workflow and limitations in sciviz tools versus Blender:

- Blender's UI and workflow are fully supportive of working creatively.
  Sciviz tools are usually centered on very functional tasks, like data
  analysis. As such, creative control in sciviz tools is usually limited.

- Blender supports advanced shading, lighting and animation, where most
  sciviz tools provide only rudimentary functionality.

But as Blender is not a scientific visualization tool it has its own
limitations in this area as well:

- Scientific visualization usually involves more than just turning data
  into visuals. It involves interactive (visual) analysis to interpret,
  understand and check the data (and visuals). Sometimes, looking at 
  the underlying numbers in the dataset is used as well.
  This is what sciviz tools like ParaView allow you to do quite easily.
  They also contains *data analysis* operations, such as integration, histogramming, 
  plot value over line and k-means.
  
- Sciviz tools like ParaView and VisIt allow analysis and visualization 
  of large datasets using parallel (multi-node) processing and rendering. 
  This usually involves a client-server setup, where the sciviz GUI application
  is the client. Blender does not provide this kind of functionality,
  although CyclesPhi is a step in this direction for rendering (but not
  analysis)
  
- Vertex colors are quite often used to show data values. In Blender adding
  vertex colors on a model is easy, but showing them in the 3D viewport
  requires the appropriate draw mode to be active (e.g. Material preview
  in EEVEE and Cycles). Also, the mesh's shader must use, for example,
  an Attribute node. Changing the value displayed is also not as easy as in 
  ParaView, as the user needs to edit the Attribute node. But this
  can be hidden using scripting and custom UI elements, where the script 
  just provides a  selection menu for the value to show and color map to use.

## Rendering

Current limitations in OSPRay compared to Cycles when it comes to 
high-quality rendering:

- No flexible shading system, i.e. no node networks, pattern generation, 
  mix shader, use of ray-state values in shaders, etc. This means 
  artistic control in this respect is limited
- A limited number of framebuffer channels, e.g. no material index,
  no lighting passes, no AO, etc. Some of these can probably be added manually,
  for example by overriding materials to pure white for an AO pass and 
  then rendering this out to a texture.
- No motion blur
- No control of the pixel filter used for the final output image
- Less control of path tracing settings, e.g. can't set limits on the 
  number of bounces per type