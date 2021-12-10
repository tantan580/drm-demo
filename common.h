#ifndef __common_drm_h
#define __common_drm_h

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct buffer_object {
	uint32_t width;
	uint32_t height;
	uint32_t pitch;
	uint32_t handle;
	uint32_t size;
	uint32_t *vaddr;
	uint32_t fb_id;
};

static int get_resources(int fd, drmModeRes **resources)
{
    *resources = drmModeGetResources(fd);
    if (*resources == NULL)
        return -1;
    return 0;
}

#define MAX_DRM_DEVICES 64

static int find_drm_device(drmModeRes **resources)
{
    drmDevicePtr devices[MAX_DRM_DEVICES] = { NULL };
    int num_devices, fd = -1;

    num_devices = drmGetDevices2(0, devices, MAX_DRM_DEVICES);
    if (num_devices < 0) {
        printf("drmGetDevices2 failed: %s\n", strerror(-num_devices));
        return -1;
    }

    for (int i = 0; i < num_devices; i++) {
        drmDevicePtr device = devices[i];
        int ret;

        if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY)))
            continue;
        /* OK, it's a primary device. If we can get the
         * drmModeResources, it means it's also a
         * KMS-capable device.
         */
        fd = open(device->nodes[DRM_NODE_PRIMARY], O_RDWR);

        printf("find_drm_device, device%s, %d\n", device->nodes[DRM_NODE_PRIMARY], fd);
        if (fd < 0)
            continue;
        ret = get_resources(fd, resources);
        if (!ret)
            break;
        close(fd);
        fd = -1;
    }
    drmFreeDevices(devices, num_devices);

    if (fd < 0)
        printf("no drm device found!\n");
    return fd;
}

//create buffer_object
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

static int modeset_create_fb_color(int fd, struct buffer_object *bo, uint32_t color)
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

    for (int i = 0; i < (bo->size / 4); i++)
		bo->vaddr[i] = color;
    return 0;
}

//destroy fb
static void modeset_destory_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_destroy_dumb destory = {};
    drmModeRmFB(fd, bo->fb_id);
    munmap(bo->vaddr, bo->size);
    //destroy.handle = bo->handle;
    drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destory);
}


#endif
