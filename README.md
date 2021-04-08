# drm-demo
DRM 模块：

1. libdrm：对底层接口进行封装，向上层提供通用的API接口，主要是对各种IOCTL接口进行封装。

2. KMS：Kernel Mode Setting，Mode Setting，包含：***更新画面和设置显示参数***。

   **更新画面**：显示buffer的切换，多图层的合成方式，以及每个图层的显示位置。

   **设置显示参数**：包括分辨率、刷新率、电源状态（休眠唤醒）等

3. GEM：Graphic Execution Manager，主要负责显示buffer的分配和释放，<u>*也是GPU唯一用到DRM的地方*</u>。

###### DRM 运行的必要条件：

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

modeset-single-buffer.c:
1) open fd
2) drmModeGetResources
3) drmModeAddFB
4) drmModeGetConnector 
5) drmModeSetCrtc  显示fb的内容