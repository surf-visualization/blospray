Exporting blender scene elements to the render server is fairly
straightforward, but there's multiple challenges in doing this efficiently
when it comes to only sending updated parts to the server. This is especially
relevant for preview rendering, where blender already provides a list of
updated data.

## Blender objects do not have a fixed identifier

Blender data blocks at the level of the Python API, or actually [IDs](https://docs.blender.org/api/blender2.8/bpy.types.ID.html)
as they're called in the documentation, can be anything from scene
elements like meshes and lights to UI elements like workspaces.

Each ID has a unique name, which, for example, is displayed in the UI
for 3D objects. Note that this name is only unique per type of datablock.
For example, you have both an 3D object and a mesh named "Cube":

```
>>> bpy.data.objects['Cube']
bpy.data.objects['Cube']

>>> bpy.data.meshes['Cube']
bpy.data.meshes['Cube']
```

Unfortunately, these names can change, e.g. through a rename by a user or because the user duplicates an object (the latter receives a new unique name based
on the parent one). And as a name isn't fixed it cannot directly be used as
an object's identity.

Strangely enough there is no other object identifier available,
except for the name value. In Python one would be tempted to use `id(obj)`
but even this doesn't return a stable value in Blender: an undo
operation might create a new data block to reconstruct an object's
earlier state, thereby changing the `id()` value associated with a name:

```
>>> bpy.data.objects['Cube']
bpy.data.objects['Cube']

>>> id(bpy.data.objects['Cube'])
139656929781896

>>> bpy.ops.object.delete()
Info: Deleted 1 object(s)
{'FINISHED'}

# Ctrl-Z in 3D view
# Note: bpy.ops.ed.undo() doesn't seem to work here

>>> id(bpy.data.objects['Cube'])
139656865164808
```

Adding object identifiers manually is also not easy. For example,
using a [custom property](https://docs.blender.org/manual/en/dev/data_system/custom_properties.html) to store an ID would be suboptimal as it gets duplicated
when an object is duplicated by a user leading to two objects with
the same ID. It also would imply tracking all scene changes to create/update
the ID values.

See [here](https://devtalk.blender.org/t/universal-unique-id-per-object/363)
and [here](https://blenderartists.org/t/unique-object-id/602113) for more information
on this issue.

## Preview rendering updates

In interactive preview rendering the user can make scene changes while the rendering
is continuously refreshed. Blender optimizes this process by providing only
the scene changes to the render engine: the [RenderEngine](https://docs.blender.org/api/blender2.8/bpy.types.RenderEngine.html)
API provides the `view_update()` method for this. The `context` parameter passed to this method has a link to the [dependency graph](https://docs.blender.org/api/blender2.8/bpy.types.Depsgraph.html) (through `depsgraph`), which can
be queried for updated datablocks through the `updates` attribute.
There's also the `id_type_updated()` method, which can be used to detect
changes in classes of objects, e.g. wether any meshes were changed.

However, the list of updates is sometimes conservative and lacks detail
on the specific changes that were done. This causes render engines
to have to handle more updating than is strictly necessary. For example:

* When translating an object this flags type OBJECT as updated, plus listing the
  specific Object being moved as updated, but does not indicate what exactly changed in the Object's properties. So a render engine is either obliged to update its copy of Object in full or try to detect what properties changed.  
* When the object selection of a scene is changed the scene datablock is listed
  in the updates, but the OBJECT type is also flagged as updated, potentially
  causing a check on all 3D objects in the scene for what has changed.
* Deleting an object flags COLLECTION, OBJECT and SCENE types as changed. However,
  the deleted object itself is not in the list of updated datablock, only the scene and collection are, so a check is needed to figure out an object was deleted. Similarly for adding an object to the scene. This will flag COLLECTION,
  OBJECT, SCENE plus the type of object added (e.g. LIGHT or MESH). In this
  case the new object (Object + ObjectData) *are* in the list of updated datablocks.
* Changing the selection in edit mode will list the object and mesh being
  edited, again without indicating only the selection has changed.
* When a mesh is edited in edit mode both the Mesh object as well as the
  Object are flagged as changed. Similarly for changing the focal length of
  a camera, etc. XXX this is actually a good thing, right?
* Editing the custom properties on a object is not directly detectable
* Switching to a different property tab, without making any scene changes,
  causes a `view_update()` call, but without any changes being flagged XXX not a problem
* Given two objects Cube and Cube.001 renaming the latter one to Cube
  (and therefore also causing the existing Cube to get renamed) does not
  list both objects in the updated datablocks list, only the object on
  which the rename was explicitly performed, plus it flags the OBJECT type as changed. The rename is therefore practically impossible to detect, unless
  you have detailed previous state of all objects to compare against.

Other suboptimal things:

* Switching out and in render preview mode will cause a new `RenderEngine`
  instance to be created, so any state stored is lost.

## BLOSPRAY specific issues

Can we detect reliably that a plugin needs to rerun? I.e. changing custom
properties on a Mesh should rerun the plugin.

Should maybe have a flag to not update plugin-based data automatically,
but only in response to a user action (e.g. button to update)
