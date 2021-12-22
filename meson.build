project(
    'drm-deom',
    'c',
	'cpp',
    version: '0.1.0',
    license: 'MIT',
    meson_version: '>=0.53.0',
	default_options: [
		'cpp_std=c++17',
		'c_std=c11',
		'warning_level=3',
		'werror=false',
	],
)

drm_dep = dependency('libdrm')
egl_dep = dependency('egl')
jpeg_dep = dependency('libjpeg')
png_dep  = dependency('libpng')

cpp = meson.get_compiler('cpp')
m_dep = cpp.find_library('m')
dl_dep = cpp.find_library('dl')


demo_srcs = {
    'modeset-single-buffer' : {
        'src' : 'modeset-single-buffer.c',
        'deps' : drm_dep,
    },
    'modeset-double-buffer' : {
        'src' : 'modeset-double-buffer.c',
        'deps' : drm_dep,
    },
    'modeset-vsync' : {
        'src' : 'modeset-vsync.c',
        'deps' : drm_dep,
    },
    'modeset-plane-test' : {
        'src' : 'modeset-plane-test.c',
        'deps' : drm_dep,
    },
    'modeset-atomic-crtc' : {
        'src' : 'modeset-atomic-crtc.c',
        'deps' : drm_dep,
    },
    'modeset-atomic-plane' : {
        'src' : 'modeset-atomic-plane.c',
        'deps' : drm_dep,
    },
    'drm-test' : {
        'src' : 'drm-test.cpp',
        'deps' : [drm_dep, m_dep, ],
    },
    'drm-tty' : {
        'src' : 'drm-tty.cpp',
        'deps' : [drm_dep, ],
    },
}

foreach name, info : demo_srcs
    executable(
        name, 
        info.get('src'),
        dependencies: info.get('deps', []),
        include_directories : include_directories('.'),
    )
endforeach