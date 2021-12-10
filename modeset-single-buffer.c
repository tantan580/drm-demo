#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buffer_object {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t handle;
    uint32_t size;
    uint8_t *vaddr;
    uint32_t fb_id;
};

struct buffer_object buf;

static int modeset_create_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb map = {};

    //create a dumb-buffer, the pixcel format is XRGB888
    create.width = bo->width;
    create.height = bo->height;
    create.bpp = 32;
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    //bind the dump-buffer to an FB object
    bo->pitch = create.pitch;
    bo->size = create.size;
    bo->handle = create.handle;

    drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch, bo->handle, &bo->fb_id);
    //map the dumb-buffer to userspace
    map.handle = create.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map.offset);

    //initialize the dumb-buffer with white-color
    memset(bo->vaddr, 0x0000ff, bo->size);
    return 0;
}

static void modeset_destory_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_destroy_dumb destory = {};
    drmModeRmFB(fd, bo->fb_id);
    munmap(bo->vaddr, bo->size);
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory);
}

int main(int argc, char **argv)
{
    int fd;
    drmModeConnector *conn;
    drmModeRes *res;
    uint32_t conn_id;
    uint32_t crtc_id;
    //open the drm device
    fd = open("/dev/dri/card0", O_RDWR);
    //fd = find_drm_device(&res);
    if (-1 == fd) {
        printf("no drm device found,fd = :%s!\n", fd);
        return -1;
    }
    //get connector for display mode
    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[0];
    conn_id = res->connectors[0];

    conn = drmModeGetConnector(fd, conn_id);
    buf.width = conn->modes[0].hdisplay;
    buf.height = conn->modes[0].vdisplay;


    modeset_create_fb(fd, &buf);

    drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);

    getchar();
    modeset_destory_fb(fd, &buf);

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    close(fd);
    return 0;
}
