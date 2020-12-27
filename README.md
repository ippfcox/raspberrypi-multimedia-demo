# RaspberryPi Multimedia Demo

## video_capture_mmap

a simple video capture demo, capture YUV to file, memory type is V4L2_MEMORY_MMAP.

capture format is 1920x1080, but output yuv may be 1920x1088.

build: 

```bash
make CROSS_COMPILE=arm-linux-gnueabihf-
```

## Acknowledge

- Makefile: [Makefile_templet](https://github.com/latelee/Makefile_templet)
- YUV Player: [YUVPlayer](https://github.com/latelee/YUVPlayer)