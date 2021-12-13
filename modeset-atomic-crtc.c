#define _GNU_SOURCE

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
#include "common.h"

struct buffer_object buf;

int main(int argc, char **argv)
{
    int fd;
    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;
    drmModeObjectProperties *props;
    drmModeAtomicReq *req;

    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t plane_id;
    uint32_t blob_id;
    uint32_t property_ctrc_id;
    uint32_t property_mode_id;
    uint32_t property_active;

    //open the drm device
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    //fd = find_drm_device(&res);
    if (-1 == fd) {
        printf("no drm device found,fd = :%d!\n", fd);
        return -1;
    }

    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    plane_res = drmModeGetPlaneResources(fd); 
    plane_id = plane_res->planes[0];
    printf("plane_id : %d --> count_planes : %d \n", plane_id, plane_res->count_planes);

    conn = drmModeGetConnector(fd, conn_id);
    buf.width = conn->modes[0].hdisplay;
    buf.height = conn->modes[0].vdisplay;
    printf("width: %d-----> height: %d \n", buf.width, buf.height);

    modeset_create_fb(fd, &buf);
    drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

    //get connector properties
    props = drmModeObjectGetProperties(fd, conn_id,	DRM_MODE_OBJECT_CONNECTOR);
    property_crtc_id = get_property_id(fd, props, "CRTC_ID");
    drmModeFreeObjectProperties(props);

    //get crtc properties
    props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
    property_active = get_property_id(fd, props, "ACTIVE");
	property_mode_id = get_property_id(fd, props, "MODE_ID");
    drmModeFreeObjectProperties(props);

    drmModeCreatePropertyBlob(fd, &conn->modes[0],
				sizeof(conn->modes[0]), &blob_id);

	req = drmModeAtomicAlloc();
	drmModeAtomicAddProperty(req, crtc_id, property_active, 1);
	drmModeAtomicAddProperty(req, crtc_id, property_mode_id, blob_id);
	drmModeAtomicAddProperty(req, conn_id, property_crtc_id, crtc_id);
	drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	drmModeAtomicFree(req);

	printf("drmModeAtomicCommit SetCrtc\n");
	getchar();

    int32_t crtc_x = 100;
    int32_t crtc_y = 100;
    uint32_t crtc_w =  buf.width * 0.5;
    uint32_t crtc_h = buf.height * 0.5;
    uint32_t src_x = 0; uint32_t src_y = 0;
    uint32_t src_w = buf.width;
    uint32_t src_h= buf.height;
	if (drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0,
			crtc_x, crtc_y, crtc_w, crtc_h,
			src_x, src_y, src_w << 16, src_h << 16)) {

        fprintf(stderr, "failed to enable plane: %s\n",
		strerror(errno));
    }
    
    printf("drmModeSetPlane\n");
    getchar();

    modeset_destory_fb(fd, &buf);

    drmModeFreeConnector(conn);
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(res);   

    close(fd);

    return 0;
}