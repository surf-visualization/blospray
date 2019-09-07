#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <string>
#include <ospray/ospray.h>

#include "messages.pb.h"

typedef std::vector<OSPInstance>    OSPInstanceList;
typedef std::vector<OSPLight>		OSPLightList;

enum SceneObjectType
{
    SOT_MESH,           // Blender mesh    -> rename to SOT_TRIANGLE_MESH
    SOT_GEOMETRY,       // OSPRay geometry
    SOT_VOLUME,
    SOT_SLICES,
    SOT_ISOSURFACES,
    SOT_SCENE,
    SOT_LIGHT           // In OSPRay these are actually stored on the renderer, not in the scene    // XXX not used?
};

enum SceneDataType
{
    SDT_PLUGIN,
    SDT_MESH
};

static const char *SceneObjectType_names[] = {
	"SOT_MESH", "SOT_GEOMETRY", "SOT_VOLUME", "SOT_SLICES", "SOT_ISOSURFACES", "SOT_SCENE", "SOT_LIGHT"
};

static const char *SceneDataType_names[] = {
	"SDT_PLUGIN", "SDT_MESH"
};

struct SceneObject
{
    SceneObjectType type;                   // XXX the type of scene objects actually depends on the type of data linked

    glm::mat4       object2world;
    //json            parameters;

    std::string     data_link;              // Name of linked scene data, may be ""

    SceneObject() {}
    ~SceneObject() {}
};

struct SceneObjectMesh : SceneObject
{
	OSPGeometricModel geometric_model;
	OSPGroup group;
	OSPInstance instance;

	SceneObjectMesh(): SceneObject()
	{
		type = SOT_MESH;
		geometric_model = nullptr;
		group = ospNewGroup();
		instance = ospNewInstance(group);
	}           

	virtual ~SceneObjectMesh()
	{
		ospRelease(geometric_model);
		ospRelease(instance);
	}
};

struct SceneObjectGeometry : SceneObject
{
	OSPGeometricModel geometric_model;
	OSPGroup group;
	OSPInstance instance;

	SceneObjectGeometry(): SceneObject()
	{
		type = SOT_GEOMETRY;
		geometric_model = nullptr;
		group = ospNewGroup();
		instance = ospNewInstance(group);
	}           

	virtual ~SceneObjectGeometry()
	{
		ospRelease(geometric_model);
		ospRelease(instance);
	}
};

struct SceneObjectVolume : SceneObject
{
	OSPVolumetricModel volumetric_model;
	OSPGroup group;
	OSPInstance instance;
	// XXX TF and material

	SceneObjectVolume(): SceneObject()
	{
		type = SOT_VOLUME;
		volumetric_model = nullptr;   
		group = ospNewGroup();
		instance = ospNewInstance(group);
	}           

	virtual ~SceneObjectVolume()
	{
		ospRelease(volumetric_model);
		ospRelease(instance);
	}
};


struct SceneObjectIsosurfaces : SceneObject
{
	OSPVolumetricModel volumetric_model;
	OSPGeometry isosurfaces_geometry;
	OSPGeometricModel geometric_model;
	OSPGroup group;
	OSPInstance instance;
	// XXX TF and material

	SceneObjectIsosurfaces(): SceneObject()
	{
		type = SOT_ISOSURFACES;
		volumetric_model = nullptr;
		isosurfaces_geometry = ospNewGeometry("isosurfaces"); 
		geometric_model = ospNewGeometricModel(isosurfaces_geometry);
		group = ospNewGroup();
		OSPData data = ospNewData(1, OSP_OBJECT, &geometric_model, 0);
	        ospSetObject(group, "geometry", data);
	    ospCommit(group);
		instance = ospNewInstance(group);
	}           

	virtual ~SceneObjectIsosurfaces()
	{
		ospRelease(volumetric_model);
		ospRelease(instance);
	}
};


struct SceneObjectScene : SceneObject
{
	OSPInstanceList instances;
	OSPLightList lights;

	SceneObjectScene(): SceneObject()
	{
		type = SOT_SCENE;
	}           

	virtual ~SceneObjectScene()
	{
        for (OSPInstance& i : instances)
            ospRelease(i);
        for (OSPLight& l : lights)
            ospRelease(l);
	}
};

struct SceneObjectLight : SceneObject
{
	OSPLight light;
	LightSettings::Type light_type;

	SceneObjectLight(): SceneObject()
	{
		type = SOT_LIGHT;
		light = nullptr;   
	}           

	virtual ~SceneObjectLight()
	{
		ospRelease(light);
	}
};



#endif
