#include <iostream>
#include <fstream>
#include <algorithm>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <GLFW/glfw3.h>

#include <gbm.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>

#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "render/shader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define XRES 800
#define YRES 600
#define BPP 32

struct DisplayContext {
    int fd;   //drm device handle
    drmModeModeInfo mode;
    EGLDisplay egl_display;

    uint32_t conn; //connector id
    uint32_t crtc; //crtc id

    struct gbm_surface *gbm_surface;
    EGLSurface egl_surface;

    struct gbm_bo *bo;
    struct gbm_bo *next_bo;
    uint32_t next_fb_id;

    bool pflip_pending;
    bool cleanup;
};

struct Drm {
    drmModeRes *res;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
};

static DisplayContext dc;
static Drm drm;

static int initDrm()
{
    //open the drm device
    dc.fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    //fd = find_drm_device(&res);
    if (-1 == dc.fd) {
        printf("no drm device found,fd = :%d!\n", dc.fd);
        return -1;
    }
    
    //acquire drm resources
    drm.res = drmModeGetResources(dc.fd);
    if (drm.res == 0) {
        printf("drmModeGetResources failed. \n");
        exit(0);
    }
    
    //acquire drm connector
    int i;
    for (i = 0; i < drm.res->count_connectors; ++i) {
        drm.connector = drmModeGetConnector(dc.fd,drm.res->connectors[i]);
        if (drm.connector == 0) {
            continue;
        }
        if (drm.connector->connection == DRM_MODE_CONNECTED && drm.connector->count_modes) {
            dc.conn = drm.connector->connector_id;
            std::cerr << "find connected connector id: " << dc.conn <<std::endl;
            break;
        }
        drmModeFreeConnector(drm.connector);
    }
    
    if (i == drm.res->count_connectors) {
        printf("No active connector found!");
        exit(0);
    }

    //crtc matching encode 
    drm.encoder = NULL;
    if (drm.connector->encoder_id) {
        drm.encoder = drmModeGetEncoder(dc.fd, drm.connector->encoder_id);
        if (drm.encoder) {
            dc.crtc = drm.encoder->crtc_id;
            drmModeFreeEncoder(drm.encoder);
        }
    }

    if (!drm.encoder) {
        for (i = 0; i < drm.res->count_encoders; ++i) {
            drm.encoder = drmModeGetEncoder(dc.fd, drm.res->encoders[i]);
            if (drm.encoder == 0) {
                continue;
            }
            for (int j = 0; j < drm.res->count_crtcs; ++j) {
                if (drm.encoder->possible_crtcs & ( 1<<j )) {
                    dc.crtc = drm.res->crtcs[j];
                    break;
                }
            }
            
            drmModeFreeEncoder(drm.encoder);
            if (dc.crtc) break;
        }
    }
    std::cerr << "find crtc id: " << dc.crtc <<std::endl << "find encoder :" << 
            drm.encoder->encoder_id <<std::endl;
    if (i == drm.res->count_encoders) {
        printf("No active encoder found!"); exit(0); 
    }
    
    //check for requested mode
    for (i = 0; i < drm.connector->count_modes; i++) {
        dc.mode = drm.connector->modes[i];
        printf("1dc.mode.hdisplay->%d\n",dc.mode.hdisplay);
        if ( (dc.mode.hdisplay==XRES) && (dc.mode.vdisplay==YRES) ) {
            printf("dc.mode.hdisplay->%d\n",dc.mode.hdisplay);
            break; 
        }
    }
    if (i == drm.connector->count_modes) {
        printf("Requested mode not found!"); exit(0); 
    }
    dc.mode = drm.connector->modes[0];
    printf("\tMode chosen [%s] : Clock => %d, Vertical refresh => %d, Type => %d\n",
            dc.mode.name, dc.mode.clock, dc.mode.vrefresh, dc.mode.type);
    printf("**************************** \n");
}

static EGLContext ctx;
//使用gbm+egl初始化EGLDisplay, EGLSurface
static int initEgl()
{
    //egl settings + gbm
    struct gbm_device *gbm = gbm_create_device(dc.fd);
    printf("backend name: %s\n",gbm_device_get_backend_name(gbm));
    printf("***************************************************************\n");

    dc.gbm_surface = gbm_surface_create(gbm, dc.mode.hdisplay, dc.mode.vdisplay, 
                        GBM_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    
    if (!dc.gbm_surface) {
        printf("cannot create gbm surface (%d): %m\n", errno);
        exit(-EFAULT);
    }

    EGLint major;
    EGLint minor;
    const char *ver, *extensions;

    static const EGLint conf_att[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE,        
    };

    static const EGLint ctx_att[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE
    };

    dc.egl_display = eglGetDisplay(gbm);
    eglInitialize(dc.egl_display, &major, &minor);
    ver = eglQueryString(dc.egl_display, EGL_VERSION);
    extensions = eglQueryString(dc.egl_display, EGL_EXTENSIONS);
    fprintf(stderr, "egl_version: %s,\next: %s\n", ver, extensions);

    if (!strstr(extensions, "EGL_KHR_surfaceless_context")) {
        fprintf(stderr, "%s\n", "need EGL_KHR_surfaceless_context extension");
        exit(1);
    }
    if (!eglBindAPI(EGL_OPENGL_ES_API)) {
        std::cerr << "bind api failed" << std::endl;
        exit(-1);   
    }

    EGLConfig conf;
    int num_conf;
    EGLBoolean ret = eglChooseConfig(dc.egl_display, conf_att, &conf, 1, &num_conf);
    if (!ret || num_conf != 1) {
        printf("cannot find a proper EGL framebuffer configuration");
        exit(-1);
    }

    ctx = eglCreateContext(dc.egl_display, conf, EGL_NO_CONTEXT, ctx_att);
    if (ctx == EGL_NO_CONTEXT) {
        printf("no context created.\n");
        return -1;
    }
    dc.egl_surface = eglCreateWindowSurface(dc.egl_display, conf, 
            (EGLNativeWindowType)dc.gbm_surface, NULL);
    if (dc.egl_surface == EGL_NO_SURFACE) {
        printf("cannot create EGL window surface \n");
        return -1;
    }
    //调用 eglMakeCurrent 把 EGLDisplay， EGLSurface 和 EGLContext 绑定起来
    if (!eglMakeCurrent(dc.egl_display, dc.egl_surface, dc.egl_surface, ctx)) {
        printf("cannot activate EGL context");
        return -1;
    }
    //egl 初始化完成后，opengles 的绘图操作将会渲染到eglsurface相应的backbuffer中
    //调用 eglSwapBuffers，back buffer 的内容和 front buffer交换，图形呈现在显示屏上
    return 0;    
}

static Shader shader;
static int initOpenGL(int width, int height)
{
    char *vs = shader.load_shader("./render/vs.glsl");
    char *fs = shader.load_shader("./render/fs.glsl");
    GLuint program = shader.create_program(vs, fs);

    free(vs);
    free(fs);
    //set vbo
    GLuint vbo;
    {
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        GLfloat vertex_data[] = {
            -0.5, -0.5,
            -0.5, 0.5,
            0.5, 0.5,

            0.5, 0.5,
            0.5, -0.5,
            -0.5, -0.5,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data), vertex_data, GL_STATIC_DRAW);

        glUseProgram(program);
        GLint pos_attrib = glGetAttribLocation(program, "position");
        glEnableVertexAttribArray(pos_attrib);
        glVertexAttribPointer(pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, NULL);    
        //projection
        GLint projMId = glGetUniformLocation(program, "projM");
        glm::mat4 projM = glm::perspective(60.0f, 640.0f / 480.0f, 1.0f, 10.0f);
        glUniformMatrix4fv(projMId, 1, GL_FALSE, glm::value_ptr(projM));
        GLint viewMId = glGetUniformLocation(program, "viewM");
        //view
        auto viewM = glm::lookAt(
        glm::vec3(1.0f, 1.0f, 1.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f));
        glUniformMatrix4fv(viewMId, 1, GL_FALSE, glm::value_ptr(viewM));
        auto resolution = glm::vec3(width, height, 1.0);
        glUniform3fv(glGetUniformLocation(program, "resolution"),
                 1, glm::value_ptr(resolution));    
    }
    return 0;
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
    uint32_t fb_id = (uint32_t)(unsigned long)data;
    drmModeRmFB(dc.fd, fb_id);
    std::cerr << __func__ << " destroy fb " << fb_id << std::endl;
}

static uint32_t bo_to_fb(gbm_bo *bo)
{
    //将bo转成fb
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t stride = gbm_bo_get_stride(bo);
    int width = gbm_bo_get_width(bo);
    int height = gbm_bo_get_height(bo);

    uint32_t fb_id = 0;
    int ret = drmModeAddFB(dc.fd, width, height, 24, 32, stride, handle, &fb_id);
    gbm_bo_set_user_data(bo, (void *)(unsigned long)fb_id, drm_fb_destroy_callback);
    printf("add new fb = %u\n", fb_id);
    if(ret) { printf("Could not add framebuffer(%d)!", errno); exit(0); }  
    return fb_id; 
}

static void modeset_page_flip_handler(int fd, unsigned int frame,
        unsigned int sec, unsigned int usec,
        void *data)
{
    std::cerr << __func__ << " frame: " << frame << std::endl;
    dc.pflip_pending = false;
}
static struct timeval first_time = {0, 0};
static void render()
{
    struct timeval tm_start, tm_endl;
    gettimeofday(&tm_start, nullptr);

    // render_screen
    {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glUseProgram(shader.program);
        struct timeval tm;
        gettimeofday(&tm, nullptr);
        if (!first_time.tv_sec) {
            first_time =  tm;
        }
        float timeval = (tm.tv_sec - first_time.tv_sec) +
        (tm.tv_usec - first_time.tv_usec) / 1000000.0;
        GLint time = glGetUniformLocation(shader.program, "time");
        glUniform1f(time, timeval);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    if (!eglSwapBuffers(dc.egl_display, dc.egl_surface)) {
        printf("cannot swap buffers\n");
        exit(-1);
    }

    dc.next_bo = gbm_surface_lock_front_buffer(dc.gbm_surface);
    //printf("next_bo = %lu\n", (unsigned long)dc.next_bo);
    if (!dc.next_bo) {
        printf("2.cannot lock front buffer during creation\n");
        exit(-1);
    }

    uint32_t bo_fb = (uint32_t)(unsigned long)gbm_bo_get_user_data(dc.next_bo);
    if (bo_fb) {
        dc.next_fb_id = bo_fb;
    } else {
        dc.next_fb_id = bo_to_fb(dc.next_bo);
    }
    //printf("dc.next_fb_id = %u\n", dc.next_fb_id);
    //将另一个frame buffer送给crtc，不断循环，实现两个frame buffer的之间的切换
    auto ret = drmModePageFlip(dc.fd, dc.crtc, dc.next_fb_id, 
            DRM_MODE_PAGE_FLIP_EVENT, NULL);
}

static void draw_loop()
{
    int fd = dc.fd;
    int ret;
    fd_set fds;
    drmEventContext ev;
    FD_ZERO(&fds);
    memset(&ev, 0, sizeof(ev));
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = modeset_page_flip_handler;

    while (1)
    {
        printf("dc.pflip_pending--> %d\n", dc.pflip_pending);
        dc.pflip_pending = true;
        render();
        while (dc.pflip_pending) {
            FD_SET(0, &fds);
            FD_SET(fd, &fds);
            //该函数不使用会导致闪烁
            drmHandleEvent(fd, &ev); 
        }
        
        if (dc.next_bo) {
            gbm_surface_release_buffer(dc.gbm_surface, dc.bo);
            dc.bo = dc.next_bo;
        }
    }
    
}

int main(int argc, char **argv)
{
    if (initDrm() == -1) exit(-1);
    //使用gbm+egl初始化EGLDisplay, EGLSurface
    if (initEgl() == -1) exit(-1);
    //设置传入的数据-->顶点数据，shader编译链接生成program
    initOpenGL(dc.mode.hdisplay, dc.mode.vdisplay);
    //必须要swapbuffer后,dc.bo才能获取到值
    if (!eglSwapBuffers(dc.egl_display, dc.egl_surface)) {
        printf("cannot swap buffers");
        exit(-1);
    }
    dc.bo = gbm_surface_lock_front_buffer(dc.gbm_surface);
    printf("first_bo = %lu\n", (unsigned long)dc.bo);
    if (!dc.bo) {
        printf("cannot lock front buffer during creation \n");
        exit(-1);
    }

    uint32_t fb_id = bo_to_fb(dc.bo);
    printf("begin drm set crtc.\n");
    //如果不调用drmModeSetCrtc则是离屏的
    int ret = drmModeSetCrtc(dc.fd, dc.crtc, fb_id, 0, 0, &dc.conn, 1, &dc.mode);
    if (ret) {
        printf("Could not set mode!");
        exit(0);
    }
    draw_loop();
   
    drmModeFreeConnector(drm.connector);
    drmModeFreeResources(drm.res);
    return 0;
}