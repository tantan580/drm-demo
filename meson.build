project('drm-deom', 'c')

drmdep = dependency('libdrm')
executable('drm-singlebuffer', 'modeset-single-buffer.c',
            dependencies : drmdep)

executable('drm-doublebuffer', 'modeset-double-buffer.c',dependencies : drmdep)