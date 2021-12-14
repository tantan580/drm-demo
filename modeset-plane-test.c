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
#include <signal.h>
#include "common.h"

struct buffer_object buf;

int main(int argc, char **argv)
{
    int fd;
    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;
	uint32_t conn_id;
	uint32_t crtc_id;
    uint32_t plane_id;

    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
    //fd = find_drm_device(&res);
    printf("fd :%d \n", fd);
    if (-1 == fd) {
        printf("no drm device found,fd = :%d!\n", fd);
        return -1;
    }

    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[0];
	conn_id = res->connectors[0];
    printf("crtc_id : %d -->\n", crtc_id);

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    plane_res = drmModeGetPlaneResources(fd);
    if (!plane_res) {
		fprintf(stderr, "drmModeGetPlaneResources failed: %s\n",strerror(errno));
		return res;
	}
    //在honor机器上运行./build/tests/modetest/modetest -P 52@62:1920x1080
    //发现plane52号和crtc62号是绑定的，因此plane_id必须是52号
    for (int i = 0; i < plane_res->count_planes; i++) {
        drmModePlane *ovr = drmModeGetPlane(fd, plane_res->planes[i]);
        if (!ovr)
            continue;
        printf("crtc_id: %d, plane_id: %d, fb_id: %d, crtc_x : %d, crtc_y: %d, x: %d, y: %d\n",
            ovr->crtc_id, ovr->plane_id, ovr->fb_id, ovr->crtc_x, ovr->crtc_y, ovr->x, ovr->y);
        printf("----------------------------------------\n");
        if (ovr->crtc_id == crtc_id) {
            plane_id = plane_res->planes[i];
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
    drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);

    getchar();
    
    //crop the rect from framebuffer(100, 150) to crtc(50,50)
    /*
    drmModeSetPlane(plane_id, crtc_id, fb_id, 0,
			crtc_x, crtc_y, crtc_w, crtc_h,
			src_x<<16, src_y<<16, src_w << 16, src_h << 16);
    */
    int32_t crtc_x = 100;
    int32_t crtc_y = 100;
    uint32_t crtc_w =  buf.width * 0.5;
    uint32_t crtc_h = buf.height * 0.5;
    uint32_t src_x = 0; uint32_t src_y = 0;
    uint32_t src_w = buf.width;
    uint32_t src_h= buf.height;
    //在honor机器上运行./build/tests/modetest/modetest -P 52@62:1920x1080
    //发现plane52号和crtc62号是绑定的，因此plane_id必须是52号drmModeSetPlane函数才有效
	if (drmModeSetPlane(fd, plane_id, crtc_id, buf.fb_id, 0,
			crtc_x, crtc_y, crtc_w, crtc_h,
			src_x, src_y, src_w << 16, src_h << 16)) {

        fprintf(stderr, "failed to enable plane: %s\n",
		strerror(errno));
    }

    getchar();        
    modeset_destroy_fb(fd, &buf);

    drmModeFreeConnector(conn);
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(res);

    close(fd);
    return 0;
}