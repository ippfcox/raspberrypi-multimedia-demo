#include <stdio.h>
#include <linux/videodev2.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <poll.h>

#define VIDEO_DEV "/dev/video0"
#define OUT_FILENAME "test.yuv"
#define BUFFER_NUM 4

typedef struct
{
    void *start;
    int length;
    int offset;
} buffer_mmap;

typedef struct
{
    int video_fd;
    FILE *out_fp;
    buffer_mmap buffer_mmaps[BUFFER_NUM];
} vcap_context;

int query_cap(int video_fd)
{
    struct v4l2_capability v4l2_cap;
    memset(&v4l2_cap, 0, sizeof(struct v4l2_capability));
    if (ioctl(video_fd, VIDIOC_QUERYCAP, &v4l2_cap) < 0)
    {
        printf("v4l2 query capability failed\n");
        return -1;
    }

    printf("driver: %s\n"
           "card: %s\n"
           "bus_info: %s\n"
           "version: %u\n"
           "capabilities: %x\n"
           "device_caps: %x\n"
           "V4L2_CAP_VIDEO_CAPTURE: %d\n"
           "V4L2_CAP_VIDEO_CAPTURE_MPLANE: %d\n",
           v4l2_cap.driver, v4l2_cap.card, v4l2_cap.bus_info, v4l2_cap.version,
           v4l2_cap.capabilities, v4l2_cap.device_caps,
           v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE,
           v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE);

    return 0;
}

int set_format(int video_fd)
{
    struct v4l2_format v4l2_fmt;
    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_CAP_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix.field = V4L2_FIELD_NONE;
    v4l2_fmt.fmt.pix.width = 1920;
    v4l2_fmt.fmt.pix.height = 1080;
    v4l2_fmt.fmt.pix.bytesperline = 1920; // FIXME: ?? bits alignment?
    v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;

    if (ioctl(video_fd, VIDIOC_S_FMT, &v4l2_fmt) < 0)
    {
        printf("v4l2 set format failed: %m\n");
        return -1;
    }

    return 0;
}

int get_format(int video_fd)
{
    struct v4l2_format v4l2_fmt;
    memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
    v4l2_fmt.type = V4L2_CAP_VIDEO_CAPTURE;

    if (ioctl(video_fd, VIDIOC_G_FMT, &v4l2_fmt) < 0)
    {
        printf("v4l2 get format failed: %m\n");
        return -1;
    }

    printf("width: %d\n"
           "height: %d\n"
           "sizeimage: %d\n",
           v4l2_fmt.fmt.pix.width, v4l2_fmt.fmt.pix.height, v4l2_fmt.fmt.pix.sizeimage);

    return 0;
}

int request_buffer_mmap(int video_fd)
{
    struct v4l2_requestbuffers v4l2_req;
    memset(&v4l2_req, 0, sizeof(struct v4l2_requestbuffers));
    v4l2_req.memory = V4L2_MEMORY_MMAP;
    v4l2_req.type = V4L2_CAP_VIDEO_CAPTURE;
    v4l2_req.count = BUFFER_NUM;

    if (ioctl(video_fd, VIDIOC_REQBUFS, &v4l2_req) < 0)
    {
        printf("v4l2 request buffer failed: %m\n");
        return -1;
    }

    return 0;
}

int q_buffer_mmap(int video_fd, buffer_mmap *buffer_mmaps, int index)
{
    struct v4l2_buffer v4l2_buf;
    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    v4l2_buf.type = V4L2_CAP_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;
    v4l2_buf.index = index;
    v4l2_buf.m.offset = buffer_mmaps[index].offset;
    v4l2_buf.length = buffer_mmaps[index].length;

    if (ioctl(video_fd, VIDIOC_QBUF, &v4l2_buf) < 0)
    {
        printf("v4l2 Q buffer failed: %m\n");
        return -1;
    }

    return 0;
}

int dq_buffer_mmap(int video_fd)
{
    struct v4l2_buffer v4l2_buf;
    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    v4l2_buf.type = V4L2_CAP_VIDEO_CAPTURE;
    v4l2_buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(video_fd, VIDIOC_DQBUF, &v4l2_buf) < 0)
    {
        printf("v4l2 DQ buffer failed: %m\n");
        return -1;
    }

    return v4l2_buf.index;
}

int query_buffer_mmap(int video_fd, buffer_mmap *buffer_mmaps)
{
    struct v4l2_buffer v4l2_buf;

    for (int i = 0; i < BUFFER_NUM; i++)
    {
        memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
        v4l2_buf.type = V4L2_CAP_VIDEO_CAPTURE;
        v4l2_buf.memory = V4L2_MEMORY_MMAP;
        v4l2_buf.index = i;

        if (ioctl(video_fd, VIDIOC_QUERYBUF, &v4l2_buf) < 0)
        {
            printf("v4l2 query buffer failed: %m\n");
            return -1;
        }

        buffer_mmaps[i].start = mmap(NULL, v4l2_buf.length, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, video_fd, v4l2_buf.m.offset);
        buffer_mmaps[i].length = v4l2_buf.length;
        buffer_mmaps[i].offset = v4l2_buf.m.offset;

        printf("buffer mmap, index: %d, start: %p, length: %d\n", i, buffer_mmaps[i].start,
               buffer_mmaps[i].length);

        if (q_buffer_mmap(video_fd, buffer_mmaps, i) != 0)
        {
            printf("initial Q buffer failed\n");
            return -1;
        }
    }

    return 0;
}

int stream_on(int video_fd)
{
    enum v4l2_buf_type type = V4L2_CAP_VIDEO_CAPTURE;
    if (ioctl(video_fd, VIDIOC_STREAMON, &type) < 0)
    {
        printf("v4l2 stream on failed: %m\n");
        return -1;
    }

    return 0;
}

int capture(int video_fd, buffer_mmap *buffer_mmaps, FILE *out_fp)
{
    struct pollfd fds[1];
    fds[0].fd = video_fd;
    fds[0].events = POLLIN;

    while (1)
    {
        if (poll(&fds[0], 1, 500) && fds[0].revents & POLLIN)
        {
            int index = dq_buffer_mmap(video_fd);
            if (index < 0)
            {
                printf("DQ buffer failed\n");
                return -1;
            }

            printf("v4l2 DQ buffer successfully, index: %d\n", index);

            fwrite(buffer_mmaps[index].start, 1, buffer_mmaps[index].length, out_fp);

            if (q_buffer_mmap(video_fd, buffer_mmaps, index) < 0)
            {
                printf("Q buffer failed\n");
                return -1;
            }
        }
        else
        {
            printf("waiting for device IO...\n");
        }
    }
}

vcap_context *context_create()
{
    vcap_context *ctx = (vcap_context *)malloc(sizeof(vcap_context));
    if (ctx == NULL)
    {
        printf("video capture context malloc failed\n");
        return NULL;
    }

    if ((ctx->out_fp = fopen(OUT_FILENAME, "w+")) == NULL)
    {
        printf("open yuv file <%s> failed: %m", OUT_FILENAME);
        return NULL;
    }

    if ((ctx->video_fd = open(VIDEO_DEV, O_RDWR)) <= 0)
    {
        printf("open video device <%s> failed: %m\n", VIDEO_DEV);
        return NULL;
    }

    if (query_cap(ctx->video_fd) != 0)
    {
        printf("query capabilities failed\n");
    }

    if (set_format(ctx->video_fd) != 0)
    {
        printf("set format failed\n");
        return NULL;
    }

    if (get_format(ctx->video_fd) != 0)
    {
        printf("get format failed\n");
        return NULL;
    }

    if (request_buffer_mmap(ctx->video_fd) != 0)
    {
        printf("request buffer mmap failed\n");
        return NULL;
    }

    if (query_buffer_mmap(ctx->video_fd, ctx->buffer_mmaps) != 0)
    {
        printf("query buffer mmap failed\n");
        return NULL;
    }

    if (stream_on(ctx->video_fd) != 0)
    {
        printf("stream on failed\n");
        return NULL;
    }

    return ctx;
}

void context_destroy(vcap_context *ctx)
{
    if (ctx == NULL)
    {
        return;
    }

    fclose(ctx->out_fp);

    close(ctx->video_fd);

    free(ctx);
}

int main()
{
    vcap_context *ctx = context_create();
    if (ctx == NULL)
    {
        printf("video capture context create failed\n");
        return -1;
    }

    capture(ctx->video_fd, ctx->buffer_mmaps, ctx->out_fp);

    context_destroy(ctx);

    return 0;
}