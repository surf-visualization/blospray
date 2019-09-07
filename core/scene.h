#ifndef SCENE_H
#define SCENE_H

#include <vector>
#include <string>
#include <ospray/ospray.h>

#include "messages.pb.h"

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

typedef std::vector<OSPInstance>    OSPInstanceList;
typedef std::vector<OSPLight>		OSPLightList;

struct SceneObject
{
    SceneObjectType type;                   // XXX the type of scene objects actually depends on the type of data linked

    glm::mat4       object2world;
    //json            parameters;

    std::string     data_link;              // Name of linked scene data, may be ""

    SceneObject()
    {
    }

    virtual ~SceneObject() 
    {
    }
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
