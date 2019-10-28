# Some notes and thoughts on using Blender for scientific visualization

## Introduction

After presenting BLOSPRAY at BCON19 and based on reactions and 
discussions that followed I wanted to put down some thoughts in this page.

There's quite a number of ongoing activities to make Blender more 
usable for scientific visualization:

- [BVtkNodes](https://github.com/simboden/BVtkNodes) (and the more
  active [fork](https://github.com/tkeskita/BVtkNodes]) for allowing
  VTK operations to be performed using node networks and use their
  output in a Blender scene
- [Integration of COVISE in Blender](http://blender.it4i.cz/scientific-visualization/covise/),
  for data processing and analysis
- And this project, BLOSPRAY, to make OSPRay-based rendering of scientific
  data available in Blender
  
The reasons for these activities have to do with the different focus
and limitations in sciviz tools versus Blender:

- 
tools and the strengths of Blender in areas like creative and interactive
workflows, high-quality rendering and animation support.

## Scientific visualization in Blender

- 

- Scientific visualization is usually more than just turning data
  into visuals. You want to do analysis to a certain extent to interpret,
  understand and chexk the visuals, or even look at the underlying numbers. 
  This is what sciviz tools like ParaView allow you to do quite easily.
  
- Vertex colors are quite often used to show data. In Blender adding
  vertex colors on a model is easy, but showing them in the 3D viewport
  requires the appropriate draw mode to be active (e.g. Material preview
  in EEVEE and Cycles) and the mesh's shader must use, for example,
  an Attributenode. Changing the value displayed is also not as easy as in 
  ParaView, as the user needs to edit the Attribute node. But this
  can be hidden using scripting, where the script just provides a 
  selection menu for the value to show and color map to use.

## Rendering

Current limitations in OSPRay compared to Cycles when it comes to 
high-quality rendering:

- No flexible shading system, i.e. no node networks, pattern generation, 
  mix shader, use of ray-state values in shaders, etc. This means 
  artistic control in this respect is limited
- A limited number of framebuffer channels, e.g. no material index,
  no lighting passes, no AO, etc. These can probably be added by
  overriding materials to pure white for an AO pass and then rendering
  this out to a texture.
- No motion blur
- No control of the pixel filter used for the final output image
- Less control of path tracing settings, e.g. can't set limits on the 
  number of bounces per type