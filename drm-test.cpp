/**
* used to test drm api
* g++ -std=c++1y `pkg-config libdrm --libs --cflags ` drm-test.c
*/

#define _GNU_SOURCE
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <cstdio>
#include <cstdlib>
#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <numeric>

static int fd = -1;

int main(int argc, char *argv[])
{
    if (!drmAvailable()) {
        std::cerr << "drm not available" <<std::endl;
        return -1;
    }

    fd = drmOpen("i915", nullptr);
    if (fd < 0) {
        std::cerr << "drmOpen failed, try open again" << std::endl;
        fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
        if (fd <= 0) {
            std::cerr << "drm open failed" << std::endl;
            return -1;
        }
    }

    float fps = 0.0;
    std::vector<int> intervals;
    drmVBlank vblk;
    vblk.request.sequence = 1;
    vblk.request.type = DRM_VBLANK_RELATIVE;

    struct timeval tv = {0};
    bool quit = false;
    int limit = 100;
    while (!quit && limit-- > 0) {
        int ret = drmWaitVBlank(fd, &vblk);

        int old = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        int sec = vblk.reply.tval_sec * 1000 + vblk.reply.tval_usec / 1000;

        std::cout<<"sec: " << sec << " old: " << old <<std::endl;

        std::cerr << "drmWaitVBlank reply: seq " << vblk.reply.sequence
                <<", interval： " << sec - old << std::endl;

        vblk.request.type = DRM_VBLANK_RELATIVE;
        vblk.request.sequence = 1;

        tv.tv_sec = vblk.reply.tval_sec;
        tv.tv_usec = vblk.reply.tval_usec;

        if (old) intervals.emplace_back(sec - old);        
    }

    auto sum = std::accumulate(intervals.begin(), intervals.end(), 0);
    std::cerr << "sum fps: " << sum << "   size: " << intervals.size()<< std::endl;
    fps = 1000.0f * intervals.size() / sum;
    std::cerr << "estimated fps: " << fps << std::endl;

    drmClose(fd);

    return 0;
}