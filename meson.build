project('drm-deom', 'c')

drmdep = dependency('libdrm')

executable('modeset-single-buffer', 'modeset-single-buffer.c', 
                dependencies : drmdep)

executable('modeset-doublebuffer', 'modeset-double-buffer.c', dependencies : drmdep)

executable('modeset-vsync', 'modeset-vsync.c', dependencies : drmdep)

executable('modeset-plane-test', 'modeset-plane-test.c', dependencies : drmdep)

executable('modeset-atomic-crtc', 'modeset-atomic-crtc.c', dependencies : drmdep)

executable('modeset-atomic-plane', 'modeset-atomic-plane.c', dependencies : drmdep)

