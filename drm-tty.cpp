#include <cstdio>
#include <cstdlib>
 
#include <sys/mman.h>
 
#include <fcntl.h>
#include <unistd.h>
 
#include <xf86drm.h>
#include <xf86drmMode.h>

drmModeModeInfo m800x600 = { 40000,800,840,968,1056,0,600,601,605,628,0,60
/*(40000*1000)/(1056*628)*/,0,0,0 }; 
//clock,hdisplay,hsync_start,hsync_end,htotal,hskew,vdisplay,vsync_start,vsync_end,vtotal,
//vsync,vrefresh((1000*clock)/(htotal*vtotal)),flags,type,name

#define XRES 800
#define YRES 600
#define BPP 32

int main(int argc, char *argv[])
{
    int fd; //drm device handle
    uint32_t fb_id; //framebuffer id
    uint32_t old_fb_id; //old framebuffer id
    int *front;  //pointer to memory mirror of framebuffer

    drmModeRes * res;
    drmModeConnector *connector;
    drmModeEncoder *encoder;
    drmModeModeInfo mode; //video mode in use
    drmModeCrtcPtr crtc; //crtc pointer

    //open the drm device
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    //fd = find_drm_device(&res);
    if (-1 == fd) {
        printf("no drm device found,fd = :%d!\n", fd);
        return -1;
    }

    res = drmModeGetResources(fd);
    if(res==0) { printf("drmModeGetResources failed"); exit(0); }

    crtc = drmModeGetCrtc(fd, res->crtcs[0]);
    old_fb_id = crtc->buffer_id;

    //connector
    int i;
    for (i = 0; i < res->count_connectors; ++i) {
        connector = drmModeGetConnector(fd, res->connectors[i]);
        if (connector ==0) continue;
        if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes >0 ) {
            printf("connected connector is -> %d!  i -> %d !\n", 
                connector->connector_id, i);
            break;
        }
        drmModeFreeConnector(connector);
    }

    if (i == res->count_connectors) {
        printf("No active connector found!"); exit(0); 
    }         
    
    //encoder
    for (i = 0; i < res->count_encoders; ++i) {
        encoder = drmModeGetEncoder(fd, res->encoders[i]);
        if (encoder == 0) continue;
        if (encoder->encoder_id == connector->encoder_id) {
            printf("encoder id is -> %d!  i -> %d !\n", 
                encoder->encoder_id, i);
            break;
        }
        drmModeFreeEncoder(encoder);
    }
    if (i == res->count_encoders) {
        printf("No active encoder found!"); exit(0); 
    }

    //mode
    for (i = 0; i < connector->count_modes; ++i) {
        mode = connector->modes[i];
        if ( (mode.hdisplay == XRES) && (mode.vdisplay == YRES) ) {
            break;
        }
    }
    if (i == connector->count_modes) {
        printf("Requested mode not found!"); exit(0);    
    }

    //set framebuffer
    struct drm_mode_create_dumb dc = { YRES, XRES, BPP, 0, 0, 0, 0 };
    i = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dc);
    if (i == 1) {
        printf("Could not create buffer object! \n");
        exit(0);
    }

    struct drm_mode_map_dumb dm = { dc.handle, 0, 0 };
    i = drmIoctl( fd, DRM_IOCTL_MODE_MAP_DUMB, &dm);
    if (i == 1) {
        printf("Could not map buffer object! \n");
        exit(0);        
    }

    front = (int *)mmap(0, dc.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, dm.offset);
    if (front == MAP_FAILED) {
        printf("Could not mirror buffer object!");
        exit(0); 
    }

    //enlist framebuffer
    i = drmModeAddFB(fd, XRES, YRES, BPP, BPP, dc.pitch, dc.handle, &fb_id);
    if (i == 1) {
        printf("Could not add framebuffer!");
        exit(0);
    }

    //set mode
    i = drmModeSetCrtc(fd, encoder->crtc_id, fb_id, 0, 0, &connector->connector_id, 1, &mode);
    if (i == 1) {
        printf("Could not set mode!");
        exit(0);
    }

    //draw testpattern
    for (i = 0; i < YRES; ++i) {
        for (int j = 0; j < XRES; j++) {
            front[i * XRES + j] = i * j;
        }       
    }
    
    //wait for enter key
    getchar();
    //copy back to front and flush front
    drmModeDirtyFB(fd, fb_id, 0, 0);
    //undo the drm setup in the correct sequence
    drmModeSetCrtc(fd,encoder->crtc_id, old_fb_id, 0, 0, &connector->connector_id, 1, &(crtc->mode));
    drmModeRmFB(fd, fb_id);
    munmap(front, dc.size);
    struct drm_mode_map_dumb dd = { dc.handle };
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB,&dd);
    drmModeFreeEncoder(encoder);
    drmModeFreeConnector(connector);
    drmModeFreeResources(res);
    close(fd);
    return 0;
}

 