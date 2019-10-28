# Some notes and thoughts on using Blender for scientific visualization

## Introduction

After presenting BLOSPRAY at BCON19 and based on reactions and 
discussions that followed, also of the other projects presented
in the sciviz panel,  I wanted to put down some thoughts in this page.

There's quite a number of ongoing activities to make Blender more 
usable for scientific visualization:

- [BVtkNodes](https://github.com/simboden/BVtkNodes) (and this more
  active [fork](https://github.com/tkeskita/BVtkNodes])) for allowing
  VTK operations to be performed using node networks and use their
  output in a Blender scene. This was initially presented at BCON18.
- [Integration of Covise in Blender](http://blender.it4i.cz/scientific-visualization/covise/),
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

- Blender's UI and workflow are fully supportive in working creatively.
  In contrast, sciviz tools are usually centered on very functional tasks, 
  like data analysis. As such, creative control and ease-of-use in this
  area in sciviz tools is usually limited.

- Blender supports advanced shading, lighting and animation, where most
  sciviz tools provide only basic functionality in these areas. Even though tools 
  like ParaView are moving a bit towards high-quality rendering, for example
  by integrating OSPRay for ray-traced rendering, this provides only a part 
  of the advanced options.
  
- The [Everything Nodes](https://wiki.blender.org/wiki/Source/Nodes/EverythingNodes)
  project that is being worked on would be a very good basis for all
  kinds of data processing and visualization pipelines in Blender.
  The BVtkNodes and Covise projects currently already provide such
  functionality, but the integration within Blender could be much improved 
  when the Everything Nodes framework is in place. This would make scientific
  data processing pipelines a first citizen within Blender and would
  combine node-based workflows with Blender's creative UI workflow.

But as Blender is not a scientific visualization tool it has its own
limitations compared to sciviz tools as well:

- Scientific visualization usually involves more than just turning data
  into visuals. It involves interactive (visual) analysis to interpret,
  understand and check the data (and visuals). Sometimes, looking at 
  the underlying numbers in the dataset is necessary as well.
  This is what sciviz tools like ParaView allow you to do quite easily.
  They also contains *data analysis* operations, such as integration, histogramming, 
  plotting a value over a 3D line and k-means clustering. Finally, data 
  related annotations are available, such as color legends and axis grids.
  
- Sciviz tools like ParaView and VisIt allow analysis and visualization 
  of large datasets using parallel (multi-node) processing and rendering. 
  This usually involves a client-server setup, where the sciviz GUI application
  is the client. Blender does not provide this kind of functionality,
  although CyclesPhi is a step in this direction for rendering (but not
  analysis). BLOSPRAY could support this mode, as OSPRay itself has
  support for distributed rendering based on MPI. The use case for large 
  datasets that need parallel processing is a bit of a niche though, 
  as new systems have many CPU cores, lots of memory and powerful GPUs.
  
- Volume rendering in Blender is currently geared towards the builtin smoke and fluid
  simulations. Some effort is going on into making import of external data
  from OpenVDB files possible. At BCON19 Stefan Werner gave an overview
  presentations of possible improvements to Cycles' volume rendering
  support (XXX youtube not online yet), some of which would also apply
  to scientific volumetric data. However, some forms of volumetric data might
  not get supported at all in the near future, such as unstructured grids, AMR meshes, 
  regular grids with varying cell sizes, volumes with vector values and point-centered
  values versus cell-centered values. These types of volumetric data are 
  well-supported in sciviz tools like ParaView and VisIt.
  
- Vertex colors are quite often used to show data values. In Blender adding
  vertex colors on a model is easy, but showing them in the 3D viewport
  requires the appropriate draw mode to be active (e.g. Material preview
  in EEVEE and Cycles). In other draw modes vertex colors are no longer
  visible.
  Also, the mesh's shader must use, for example,
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
  
Limitations of Cycles compared to OSPRay:

- OSPRay supports simple geometric shapes and their colors to defined 
  by a few data arrays. This allows large numbers of colored spheres, cylinders 
  or streamlines. Cycles, in contrast, supports only triangle meshes, 
  subdivision meshes, curves and volumes (XXX check this). So even simple 
  shapes like spheres need to be defined by a mesh of some sorts, 
  although these can approximated with a subdiv cube mesh. But adding a
  unique color to each shape involves using vertex colors.