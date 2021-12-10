# drm-demo

DRM 模块：

1. libdrm：对底层接口进行封装，向上层提供通用的API接口，主要是对各种IOCTL接口进行封装。
2. KMS：Kernel Mode Setting，Mode Setting，包含：***更新画面和设置显示参数***。

   **更新画面**：显示buffer的切换，多图层的合成方式，以及每个图层的显示位置。

   **设置显示参数**：包括分辨率、刷新率、电源状态（休眠唤醒）等
3. GEM：Graphic Execution Manager，主要负责显示buffer的分配和释放，`<u>`*也是GPU唯一用到DRM的地方* `</u>`。

###### DRM 运行的必要条件

1) DRM驱动支持MODESET；
2) DRM驱动支持dumb-buffer(即连续物理内存)；
3) DRM驱动至少支持1个CRTC，1个Encoder，1个Connector；
4) DRM驱动的Connector至少包含1个有效的drm_display_mode。

###### 基本元素

- CRTC	对显示buffer进行扫描，并产生时序信号的硬件模块，通常指Display Controller
- ENCODER	负责将CRTC输出的timing时序转换成外部设备所需要的信号的模块，如HDMI转换器或DSI Controller
- CONNECTOR	连接物理显示设备的连接器，如HDMI、DisplayPort、DSI总线，通常和Encoder驱动绑定在一起
- PLANE	硬件图层，有的Display硬件支持多层合成显示，但所有的Display Controller至少要有1个plane
- FB	Framebuffer，单个图层的显示内容，唯一一个和硬件无关的基本元素
- VBLANK	软件和硬件的同步机制，RGB时序中的垂直消影区，软件通常使用硬件VSYNC来实现
- property	任何你想设置的参数，都可以做成property，是DRM驱动中最灵活、最方便的Mode setting机制
- DUMB	只支持连续物理内存，基于kernel中通用CMA API实现，多用于小分辨率简单场景
- PRIME	连续、非连续物理内存都支持，基于DMA-BUF机制，可以实现buffer共享，多用于大内存复杂场景
- fence	buffer同步机制，基于内核dma_fence机制实现，用于防止显示内容出现异步问题。

## modeset-single-buffer

1) open fd
2) drmModeGetResources
3) drmModeAddFB
4) drmModeGetConnector
5) drmModeSetCrtc  显示fb的内容

注意：程序运行之前，请确保没有其它应用或服务占用/dev/dri/card0节点，否则将出现Permission Denied错误。

## modeset-vsync

要使用drmModePageFlip()，就必须依赖drmHandleEvent()函数，该函数内部以阻塞的形式等待底层驱动返回相应的vblank事件，以确保和VSYNC同步。需要注意的是，drmModePageFlip()不允许在1个VSYNC周期内被调用多次，否则只有第一次调用有效，后面几次调用都会返回-EBUSY错误（-16）

## modeset-plane-test

### Plane

DRM中的Plane指的是Display Controller中用于多层合成的单个硬件图层模块，属于硬件层面.**Plane是连接FB与CRTC的纽带，是内存的搬运工。**

Plane的历史

随着软件技术的不断更新，对硬件的性能要求越来越高，在满足功能正常使用的前提下，对功耗的要求也越来越苛刻。本来GPU可以处理所有图形任务，但是由于它运行时的功耗实在太高，设计者们决定将一部分简单的任务交给Display Controller去处理（比如合成），而让GPU专注于绘图（即渲染）这一主要任务，减轻GPU的负担，从而达到降低功耗提升性能的目的。于是，Plane（硬件图层单元）就诞生了。

在DRM框架中，Plane又分为如下3种类型：

| 类型    | 说明                                      |
| :------ | :---------------------------------------- |
| Cursor  | 光标图层，一般用于PC系统，用于显示鼠标    |
| Overlay | 叠加图层，通常用于YUV格式的视频图层       |
| Primary | 主要图层，通常用于仅支持RGB格式的简单图层 |
