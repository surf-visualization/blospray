import bl_ui

for propname in dir(bl_ui):
    
    if not propname.startswith('properties_'):
        continue
        
    print(propname)
    
    mod = getattr(bl_ui, propname)    
    #print(dir(mod))
    
    for panelname in dir(mod):
        
        if panelname.find('_PT_') == -1:
            continue
            
        panel = getattr(mod, panelname)
            
        if hasattr(panel, 'COMPAT_ENGINES'):
            print('    %s (%s)' % (panelname, panel.COMPAT_ENGINES))