# Based on intern/cycles/blender/blender_camera.cpp

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
        #self.pano_viewplane
        self.viewport_camera_border
        
        self.matrix = None  # XXX identity
        
        
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