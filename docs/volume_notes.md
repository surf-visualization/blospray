# Notes on volumetric dataset usage

XXX These notes are work-in-progress.

Within OSPRay there's three levels at which volumetric data and its rendering 
is handled:

1. The raw volume data. In OSPRay these are stored in generic data buffers created using `ospNewData()`.
   These buffers only contain basic properties relating to the data, such as the type
   of value (e.g. `float`), number of values (but not volume dimension) and data sharing flags.
   
2. The volume object, i.e. `OSPVolume`. This object references the data buffer
   holding the raw volume data and also holds volume-specific properties. These
   properties relate both to the volume itself (e.g. grid spacing and data range), 
   but also to rendering the volume (e.g. sampling rate to use and the
   transfer function to apply). An `OSPVolume` can directly be added to an
   OSPRay scene to have the volume get rendered in the output image.

3. Geometry derived from volumetric data, specifically isosurfaces and slice planes.
   In OSPRay these two types of derived geometry are rendered implicitly based on
   ray traversal of the volumetric data, so no explicit isosurface or slice plane geometry
   needs to be created. As such, these geometry objects contain a reference to a volume
   object but are of type `OSPGeometry` themselves. These can also be added
   to the OSPRay scene, independent from the addition of the referenced `OSPVolume` object.
   
When working with volumetric objects in Blender it makes sense to have some
flexibility in handling these three levels, mostly to allow sharing
of data were possible. For example, one might want to load a volumetric dataset
and extract multiple isosurfaces that are shown side-by-side. Or one might 
want to use both a volume rendered representation, as well as a slice plane, 
based on the same volumetric data. 

As two of the levels refer to another level sharing is conceptually easy, but in 
practice a bit suboptimal. This has to do with the embedding of the three levels
within Blender in a way that fits Blender's concepts and workflow. Blender itself
only uses two levels: "objects" which refer to "object data" [^1]. Here, the object holds general
information, such as the object-to-world 3D transformation to apply, while the object data holds the
data specific to the type of object. For example, mesh object data holds the 3D geometry and has a reference
to the material to use. As such, the mesh data is the natural place to 
query data on the 3D extent of a 3D surface, while the object referencing
the mesh data determines where in the 3D scene the mesh is located based on
the object's transformation.

Matching Blender's two levels to OSPRay's three is somewhat cumbersome, but we
can try to come up with some guiding principles and corresponding design choices:

- Reusing a volume object (level 2) is probably more common than reusing
  raw volume data (level 1). 
  
- Having a 3D representation of a volume in the scene is a necessity. This representation
can be as simple as a cube mesh for rectangular domains or a more complex shape
for unstructured volumetric grids. The most natural 3D object to use for this is
a Blender mesh. 

- Transformations of volumes should use existing 3D concepts and interaction 
(e.g. translate, scale), and so must work on objects, not object data.

- Re-using data is best done with Blender's existing method of object
instancing, in which two objects refer to the same object data. This allows 
the two objects to use independent transformations. This implies that 
volume-specific information (level 2) should be stored with the object
data (i.e. mesh as determined above).

Taken all together:

- We use a Blender cube mesh object to add an OSPRay volume/isosurface/slice plane to a scene
- At the object level a choice is made wether to render the volume directly,
  or show it as an isosurface or slice plane
- The size of the cube mesh is adjusted to match the extent of the underlying volume
- The object transformation is used to place the volume in the scene

But there are some ugly consequences of this scheme:

- To have both a volume representation as well as (say) an isosurface representation
of the same volumetric data involves having two objects (one volume rendered, one
isosurface rendered) refer to the same object data. But this implies that setting
the type of representation is done *at the object level*, which is counterintuitive 
in Blender as the representation is normally based on the type of object data linked 
to an object and not dependent on any object-level settings.

- Volumes currently don't support an arbitrary affine transformation in OSPRay.
Only for a structured grid is there a way to influence volume size and placement (but
not orientation) at the `OSPVolume` level. But this requires passing transformation
data from the (Blender) object level to the mesh level. Furthermore, pushing the transformation
down to the mesh level makes it impossible to share the volume in multiple objects,
as the mesh can only have one transformation.

- There's no natural place to specify the raw volume data to use. Setting this
at the mesh level does not allow sharing XXX.
Using an Empty and setting custom properties on it to specify the raw data
might be a workaround. But our current plugin API expects plugins to 

- Loading of volume data and construction of a `OSPVolume` are handled by
plugins, as Blender does not have any methods to offer for this. But object-level
transformation data needs to be available to the plugin if it needs to 
transform the volume. So the xform either needs to be passed (duplicate) with
the object data and object itself, OR the object needs to be transmitted
(including object data it references) before object data. Which means more
bookkeeping on the server. For geometry we use 1) pass mesh, 2) pass object referencing
that mesh.



There is actually a fourth level, which is to use volumetric data to color
geometry that it is located within the volume's extent, i.e. a form of
volumetric texturing. This is currently not supported in BLOSPRAY.

[^1]: Note that parenting could be used to provide more levels, but this doesn't
solve the actual problem.

XXX blender object nodes will make all of this much easier

XXX povray addon DOES add custom types (but uses existing blender objects
to represent the custom povray types)

