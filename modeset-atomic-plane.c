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
	uint32_t property_crtc_id;
	uint32_t property_mode_id;
	uint32_t property_active;
	uint32_t property_fb_id;
	uint32_t property_crtc_x;
	uint32_t property_crtc_y;
	uint32_t property_crtc_w;
	uint32_t property_crtc_h;
	uint32_t property_src_x;
	uint32_t property_src_y;
	uint32_t property_src_w;
	uint32_t property_src_h;

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
 	printf("crtc_id : %d --> conn_id : %d\n", crtc_id, conn_id);

    if (drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
		fprintf(stderr, "DRM universal planes unsupported %s\n", strerror(errno));
	}
    plane_res = drmModeGetPlaneResources(fd); 
    for (int i = 0; i < plane_res->count_planes; i++) {
        drmModePlane *ovr = drmModeGetPlane(fd, plane_res->planes[i]);
        if (!ovr)
            continue;
        printf("crtc_id: %d, plane_id: %d, fb_id: %d, crtc_x : %d, crtc_y: %d, x: %d, y: %d\n",
            ovr->crtc_id, ovr->plane_id, ovr->fb_id, ovr->crtc_x, ovr->crtc_y, ovr->x, ovr->y);
        printf("----------------------------------------\n");
        if (ovr->crtc_id == crtc_id) {
            plane_id = ovr->plane_id;
            break;
        } 
    }

	//plane_id = plane_res->planes[3];
    printf("plane_id : %d --> count_planes : %d \n", plane_id, plane_res->count_planes);

    conn = drmModeGetConnector(fd, conn_id);
    buf.width = conn->modes[0].hdisplay;
    buf.height = conn->modes[0].vdisplay;
    printf("width: %d-----> height: %d \n", buf.width, buf.height);

	modeset_create_fb(fd, &buf);

    if (drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
		fprintf(stderr, "Atomic modesetting unsupported, using legacy DRM interface %s\n", strerror(errno));
	}
    props = drmModeObjectGetProperties(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR);
	property_crtc_id = get_property_id(fd, props, "CRTC_ID");
    drmModeFreeObjectProperties(props);

    props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
	property_active = get_property_id(fd, props, "ACTIVE");
	property_mode_id = get_property_id(fd, props, "MODE_ID");
	drmModeFreeObjectProperties(props);
    printf("property_crtc_id : %d, property_active : %d, property_mode_id : %d\n", 
        property_crtc_id, property_active, property_mode_id);

    if (drmModeCreatePropertyBlob(fd, &conn->modes[0], sizeof(conn->modes[0]), &blob_id)) {
		fprintf(stderr, "Unable to create mode property blob %s\n", strerror(errno));
	}
    //start modeseting
    req = drmModeAtomicAlloc();
	printf("drmModeAtomicAlloc req: %p \n", req);
	if (!req) {
		fprintf(stderr, "Allocation failed %s\n", strerror(errno));
	}

	if (drmModeAtomicAddProperty(req, crtc_id, property_active, 1) < 0) {
		fprintf(stderr, "Failed to add atomic DRM property %s\n", strerror(errno));
	}
	if (drmModeAtomicAddProperty(req, crtc_id, property_mode_id, blob_id) < 0) {
		fprintf(stderr, "Failed to add atomic DRM property %s\n", strerror(errno));
	}
	if (drmModeAtomicAddProperty(req, conn_id, property_crtc_id, crtc_id) < 0) {
		fprintf(stderr, "Failed to add atomic DRM property %s\n", strerror(errno));
	}
	if (drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL) != 0 ) {
		fprintf(stderr, "drm atomic commit failure: %s\n", strerror(errno));
	}
	drmModeAtomicFree(req);
    /* 由于以上代码没有添加对 fb_id 的操作，因此它的作用只是初始化CRTC/ENCODER/CONNECTOR硬件，
    以及建立硬件链路的连接关系，并不会显示framebuffer的内容，即保持黑屏状态。
    framebuffer的显示将由后面的 drmModeSetPlane() 操作来完成。
    */
	printf("drmModeAtomicCommit SetCrtc\n");
	getchar();

    //get plane properties
	props = drmModeObjectGetProperties(fd, plane_id, DRM_MODE_OBJECT_PLANE);
	property_crtc_id = get_property_id(fd, props, "CRTC_ID");
	property_fb_id = get_property_id(fd, props, "FB_ID");
	property_crtc_x = get_property_id(fd, props, "CRTC_X");
	property_crtc_y = get_property_id(fd, props, "CRTC_Y");
	property_crtc_w = get_property_id(fd, props, "CRTC_W");
	property_crtc_h = get_property_id(fd, props, "CRTC_H");
	property_src_x = get_property_id(fd, props, "SRC_X");
	property_src_y = get_property_id(fd, props, "SRC_Y");
	property_src_w = get_property_id(fd, props, "SRC_W");
	property_src_h = get_property_id(fd, props, "SRC_H");
	drmModeFreeObjectProperties(props);

    //atomic plane update
	req = drmModeAtomicAlloc();
	drmModeAtomicAddProperty(req, plane_id, property_crtc_id, crtc_id);
	drmModeAtomicAddProperty(req, plane_id, property_fb_id, buf.fb_id);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_x, 50);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_y, 50);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_w, 1920);
	drmModeAtomicAddProperty(req, plane_id, property_crtc_h, 1080);
	drmModeAtomicAddProperty(req, plane_id, property_src_x, 0);
	drmModeAtomicAddProperty(req, plane_id, property_src_y, 0);
	drmModeAtomicAddProperty(req, plane_id, property_src_w, 1920 << 16);
	drmModeAtomicAddProperty(req, plane_id, property_src_h, 1080 << 16);
	if (drmModeAtomicCommit(fd, req, 0, NULL) != 0 ) {
		fprintf(stderr, "drm atomic commit failure %s\n", strerror(errno));
	}
	drmModeAtomicFree(req);

	printf("drmModeAtomicCommit SetPlane\n");
	getchar();
    
	modeset_destroy_fb(fd, &buf);

	drmModeFreeConnector(conn);
	drmModeFreePlaneResources(plane_res);
	drmModeFreeResources(res);

	close(fd);

	return 0; 
}

/*上面的两次 drmModeAtomicCommit() 操作可以合并成一次；
无需频繁调用 drmModeAtomicAlloc() 、drmModeAtomicFree() ，可以在第一次commit之前Alloc，
在最后一次commit之后free，也没问题；
plane update操作时，可以只add发生变化的property，其它未发生变化的properties即使没有被add，
在commit时底层驱动仍然会取上一次的值来配置硬件寄存器。
*/
