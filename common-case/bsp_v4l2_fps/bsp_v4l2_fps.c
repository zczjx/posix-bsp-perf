/*******************************************************************************
* @function name:     
*                
* @brief:          
*                
* @param:        
*                
*                
* @return:        
*                
* @comment:        
*******************************************************************************/



#include "bsp_v4l2_cap.h"

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#define V4L2_BUF_NR (4)

static struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
static struct bsp_v4l2_param v4l2_param;

int main(int argc, char **argv)
{
    int i, xres, yres;
	int err, vfd, pts = 0;
	struct timespec tp;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	int buf_idx = 0;
	char *pixformat = NULL;
	char *dev_path = NULL;

	if(argc < 4)
	{
		printf("usage: bsp_v4l2_fps [dev_path] [pixformat] [xres] [yres]\n");
		return -1;

	}

	dev_path = argv[1];
	pixformat = argv[2];
	xres = atoi(argv[3]);
	yres = atoi(argv[4]);

	
    // Setup of the dec requests
  
	vfd = bsp_v4l2_open_dev(dev_path);
	v4l2_param.fps = 30;
	
	if(0 == strcmp("mjpeg", pixformat))
	{
		v4l2_param.pixelformat = V4L2_PIX_FMT_MJPEG;
	}
	else if(0 == strcmp("yuyv", pixformat))
	{
		v4l2_param.pixelformat = V4L2_PIX_FMT_YUYV;
	}
	else if(0 == strcmp("yuv422p", pixformat))
	{
		v4l2_param.pixelformat = V4L2_PIX_FMT_YUV422P;
	}
	else
	{
		v4l2_param.pixelformat = V4L2_PIX_FMT_YUYV;
	}
	
	v4l2_param.xres = xres;
	v4l2_param.yres = yres;
	bsp_v4l2_try_setup(vfd, &v4l2_param);
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);
	bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR);
	bsp_v4l2_stream_on(vfd);

	while(++pts <= 50000)
	{
		bsp_print_fps("bsp_v4l2_fps: ", &fps, &pre_time, &curr_time);
		bsp_v4l2_get_frame(vfd, &buf_idx);
		bsp_v4l2_put_frame_buf(vfd, buf_idx);
		
	}
	
    return 0;
}


