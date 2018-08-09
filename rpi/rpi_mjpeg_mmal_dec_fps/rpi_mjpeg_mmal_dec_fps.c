#include "brcmjpeg.h"
#include "bsp_v4l2_cap.h"

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <gmodule.h>
#include <glib.h>


#define MAX_WIDTH   5000
#define MAX_HEIGHT  5000
#define MAX_ENCODED (15*1024*1024)
#define MAX_DECODED (MAX_WIDTH*MAX_HEIGHT*2)
#define X_RESOLUTION 480
#define Y_RESOLUTION 720
#define V4L2_BUF_NR (4)
#define WATER_MASK (8)

typedef struct rpi_image {
	__u32 bytes;
	char *addr;
} rpi_image;



static uint8_t encodedInBuf[MAX_ENCODED];
static uint8_t decodedBuf[MAX_DECODED];
static char outFileName[2048];

static int xres, yres;
static int v4l2_run = 0;

GQueue *image_que = NULL;
GMutex que_lock;

static gpointer *v4l2_cap_task(gpointer data);

int main(int argc, char **argv)
{
    BRCMJPEG_STATUS_T status;
    BRCMJPEG_REQUEST_T dec_request;
    BRCMJPEG_T *dec = 0;
	struct rpi_image *tmp_img = NULL;
    int i;
	int err, vfd, pts = 0;
	struct timespec tp;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	GThread *v4l2_task = NULL;
	char *out_fmt = NULL;

	if(argc < 4)
	{
		printf("usage: rpi_mjpeg_mmal_dec_fps [xres] [yres] [fmt: i420|yv12|i422|yv16|yuyv|rgba]\n");
		return -1;
	}

	xres = atoi(argv[1]);
	yres = atoi(argv[2]);
	out_fmt = argv[3];
	g_mutex_init(&que_lock);
	image_que = g_queue_new();
	// g_queue_clear(image_que);
	printf("new image queue len: %lu\n", g_queue_get_length(image_que));
	
    // Setup of the dec requests
    memset(&dec_request, 0, sizeof(dec_request));
    dec_request.input = encodedInBuf;
    dec_request.output = decodedBuf;
    dec_request.output_handle = 0;
    dec_request.output_alloc_size = MAX_DECODED;
	dec_request.buffer_width = xres;
	dec_request.buffer_height = yres;
	if(strcmp(out_fmt, "i420") == 0)
	{
		dec_request.pixel_format = PIXEL_FORMAT_I420;
	}
	else if(strcmp(out_fmt, "yv12") == 0)
	{
		dec_request.pixel_format = PIXEL_FORMAT_YV12;
	}
	else if(strcmp(out_fmt, "i422") == 0)
	{
		dec_request.pixel_format = PIXEL_FORMAT_I422;
	}
	else if(strcmp(out_fmt, "yv16") == 0)
	{
		dec_request.pixel_format = PIXEL_FORMAT_YV16;
	}
	else if(strcmp(out_fmt, "yuyv") == 0)
	{
		dec_request.pixel_format = PIXEL_FORMAT_YUYV;
	}
	else if(strcmp(out_fmt, "rgba") == 0)
	{
		dec_request.pixel_format = PIXEL_FORMAT_RGBA;
	}
	else
	{
		dec_request.pixel_format = PIXEL_FORMAT_I420;
	}
    status = brcmjpeg_create(BRCMJPEG_TYPE_DECODER, &dec);
	
    if (status != BRCMJPEG_SUCCESS)
    {
        fprintf(stderr, "could not create decoder\n");
        return -1;
    }

	v4l2_run = 1;
	v4l2_task = g_thread_new("v4l2_task", v4l2_cap_task, NULL);

	while(1)
	{

		g_mutex_lock(&que_lock);
		if(g_queue_get_length(image_que) < 1)
		{
			g_mutex_unlock(&que_lock);
			continue;
		}
		tmp_img = g_queue_pop_head(image_que);
		g_mutex_unlock(&que_lock);
		
		memcpy(encodedInBuf, tmp_img->addr, tmp_img->bytes);
		dec_request.input_size = tmp_img->bytes;
		g_free(tmp_img->addr);
		g_free(tmp_img);
		tmp_img = NULL;
		status = brcmjpeg_process(dec, &dec_request);

		if (status != BRCMJPEG_SUCCESS) 
		{
			fprintf(stderr, "could not decode \n");
			// break;
		}
		
		bsp_print_fps("mmal mjpeg hw dec", &fps, &pre_time, &curr_time);

	}

    brcmjpeg_release(dec);
	g_queue_free(image_que);

    return 0;
}

static gpointer *v4l2_cap_task(gpointer data)
{
	int vfd;
	int buf_idx = 0;
	struct rpi_image *tmp_img = NULL;
	struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
	struct bsp_v4l2_param v4l2_param;

	printf("start v4l2_cap_task!\n");
	vfd = bsp_v4l2_open_dev("/dev/video0");
	v4l2_param.fps = 30;
	v4l2_param.pixelformat = V4L2_PIX_FMT_MJPEG;
	v4l2_param.xres = xres;
	v4l2_param.yres = yres;
	bsp_v4l2_try_setup(vfd, &v4l2_param);
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);
	bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR);
	bsp_v4l2_stream_on(vfd);

	while(v4l2_run)
	{
		bsp_v4l2_get_frame(vfd, &buf_idx);
		
		g_mutex_lock(&que_lock);
		
		if(g_queue_get_length(image_que) < WATER_MASK)
		{
			tmp_img = g_new(struct rpi_image, 1);
			tmp_img->bytes = v4l2_buf[buf_idx].bytes;
			tmp_img->addr = g_malloc(tmp_img->bytes);
			memcpy(tmp_img->addr, v4l2_buf[buf_idx].addr, v4l2_buf[buf_idx].bytes);
			g_queue_push_tail(image_que, tmp_img);
		}

		g_mutex_unlock(&que_lock);

		bsp_v4l2_put_frame_buf(vfd, buf_idx);
	}

	g_thread_yield ();

}



