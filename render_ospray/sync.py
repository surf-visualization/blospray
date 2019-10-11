# Based on intern/cycles/blender/blender_camera.cpp
from mathutils import Matrix

class BlenderCamera:

    def __init__(self, b_render):
        self.nearclip = 1e-5
        
        self.type = 'CAMERA_PERSPECTIVE'
        self.ortho_scale = 1.0
        
        self.lens = 50.0

        self.aperturesize = 0.0
        self.apertureblades = 0
        self.aperturerotation = 0.0
        self.focaldistance = 10.0
  
        self.shift = [0, 0]
        self.offset = [0, 0]
        self.zoom = 1.0
        
        self.pixelaspect = [1.0, 1.0]
        self.aperture_ratio = 1.0
        
        self.sensor_fit = 'AUTO'    # AUTO, HORIZONTAL, VERTICAL
        self.sensor_width = 36.0
        self.sensor_height = 24.0
        
        self.full_width = int(b_render.resolution_x * b_render.resolution_percentage / 100)
        self.full_height = int(b_render.resolution_y * b_render.resolution_percentage / 100)
        
        # [left, right, bottom, top]
        self.border = [0.0, 1.0, 0.0, 1.0]        
        self.viewport_camera_border = [0.0, 1.0, 0.0, 1.0]
        #self.pano_viewplane
        
        self.matrix = Matrix()

    def modified(self, other):
        # XXX
        if self.type != other.type:
            return True
        if self.lens != other.lens:
            return True
        if self.full_width != other.full_width or self.full_height != other.full_height:
            return True
        if self.matrix != other.matrix:
            return True
        return False
            

    def from_view(self, b_engine, b_scene, b_v3d, b_rv3d, width, height):
        # b_engine is used in the b_ob branch (but not atm)
        self.nearclip = b_v3d.clip_start
        # clip_end
        self.lens = b_v3d.lens
        #self.shuttertime

        if b_rv3d.view_perspective == 'CAMERA':
            #ob = b_v3d.use_local_camera if b_v3d.camera else b_scene.camera
            #if ob:
            #    self.from_object(b_engine, b_ob, skip_panorama)

            # else:
            # Magic zoom formula
            zoom = b_rv3d.view_camera_zoom
            zoom = 1.4142 + zoom / 50.0
            zoom *= zoom
            self.zoom = 2.0 / zoom

            self.offset = b_rv3d.view_camera_offset

        elif b_rv3d.view_perspective == 'ORTHO':
            pass

        self.zoom *= 2.0
        self.matrix = b_rv3d.view_matrix.inverted()

    def viewplane(self, width, height):
        """
        Return viewplane, aspectratio, sensor_size
        """

        xratio = 1.0 * width * self.pixelaspect[0]
        yratio = 1.0 * height * self.pixelaspect[1]

        if self.sensor_fit == 'AUTO':
            horizontal_fit = xratio > yratio
            sensor_size = self.sensor_width
        elif self.sensor_fit == 'HORIZONTAL':
            horizontal_fit = True
            sensor_size = self.sensor_width
        else:
            horizontal_fit = False
            sensor_size = self.sensor_height

        if horizontal_fit:
            aspectratio = xratio / yratio
            xaspect = aspectratio
            yaspect = 1.0
        else:
            aspectratio = yratio / xratio
            xaspect = 1.0
            yaspect = aspectratio

        if self.type == 'CAMERA_ORTHOGRAPHIC':
            xaspect = xaspect * self.ortho_scale / (aspectratio * 2.0)
            yaspect = yaspect * self.ortho_scale / (aspectratio * 2.0)
            aspectratio = self.ortho_scale / 2.0

        if self.type == 'CAMERA_PANORAMA':
            viewplane = None
        else:
            # CAMERA_PERSPECTIVE
            # [left, right, bottom, top]
            viewplane = [-xaspect, xaspect, -yaspect, yaspect]

            # Zoom for 3D camera view
            viewplane = list(map(lambda v: v*self.zoom, viewplane))

            # Modify viewplane with camera shift and 3D camera view offset
            dx = 2.0 * (aspectratio * self.shift[0] + self.offset[0] * xaspect * 2.0)
            dy = 2.0 * (aspectratio * self.shift[1] + self.offset[1] * yaspect * 2.0)

            viewplane[0] += dx
            viewplane[1] += dx
            viewplane[2] += dy
            viewplane[3] += dy

        return viewplane, aspectratio, sensor_size


def sync_view(b_scene, b_v3d, b_rv3d, width, height):

    bcam = BlenderCamera(b_scene.render)
    bcam.from_view(None, b_scene, b_v3d, b_rv3d, width, height)
    #bcam.border
    #bcam.sync()

    return bcam

        
"""
def sync_camera(b_render, b_scene, width, height, viewname):
    
    bcam = BlenderCamera()
      
    bcam.pixelaspect = [b_render.pixel_aspect_x, b_render.pixel_aspect_y]
    #bcam.shuttertime = b_render.motion_blur_shutter
    
    if b_render.use_border:
        bcam.border = [b_render.border_min_x, b_render.border_max_x, b_render.border_min_y, b_render.border_max_y]
        
    b_ob = b_scene.camera
    #if b_ob:
    #    blender_camera_from_object(b_cam, b_engine, b_ob)
    #    b_engine.camera_model_matrix(b_ob, bcam.use_spherical_stereo, b_ob_matrix);
    #    bcam.matrix = get_transform(b_ob_matrix);
    
    blender_camera_sync(cam, bcam, width, height, viewname)
"""
