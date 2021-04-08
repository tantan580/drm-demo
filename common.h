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




#endif
