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

struct buffer_object buf[2];
static int terminate;

//page_flip
static void modeset_page_flip_handler(int fd, uint32_t frame, uint32_t sec, uint32_t usec, void *data)
{
    static int i = 0;
    uint32_t ctrc_id = *(uint32_t *)data;

    i ^= 1;

    drmModePageFlip(fd, ctrc_id, buf[i].fb_id, DRM_MODE_PAGE_FLIP_EVENT, data);

    usleep(500000);
}

static void sigint_handler(int arg)
{
    terminate = 1;
}

int main(int argc, char **argv)
{
    int fd;
    drmEventContext ev = {};
    drmModeConnector *conn;
    drmModeRes *res;
    uint32_t conn_id;
    uint32_t crtc_id;

    // register CTRL+C terminate interrupt
    signal(SIGINT, sigint_handler);

    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = modeset_page_flip_handler;
    //open the drm device
    fd = open("/dev/dri/card0", O_RDWR);
    //fd = find_drm_device(&res);
    if (-1 == fd) {
        printf("no drm device found,fd = :%d!\n", fd);
        return -1;
    }

    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[0];
    conn_id = res->connectors[0];

    conn = drmModeGetConnector(fd, conn_id);
    buf[0].width = conn->modes[0].hdisplay;
	buf[0].height = conn->modes[0].vdisplay;
	buf[1].width = conn->modes[0].hdisplay;
	buf[1].height = conn->modes[0].vdisplay;

    modeset_create_fb_color(fd, &buf[0], 0xff00ff);
	modeset_create_fb_color(fd, &buf[1], 0x00ff0f);

    drmModeSetCrtc(fd, crtc_id, buf[0].fb_id, 0, 0, &conn_id, 1, &conn->modes[0]);
    drmModePageFlip(fd, crtc_id, buf[0].fb_id, DRM_MODE_PAGE_FLIP_EVENT, &crtc_id);

    while (!terminate) {
        drmHandleEvent(fd, &ev);
    }
    
    modeset_destroy_fb(fd, &buf[1]);
    modeset_destroy_fb(fd, &buf[0]);

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    close(fd);
    return 0;
}
