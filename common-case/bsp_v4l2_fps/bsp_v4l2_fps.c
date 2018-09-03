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

#include <gmodule.h>

#define V4L2_BUF_NR (4)
#define V4L2_MAX_FMT (128)

static void init_v4l2_name_fmt_set(GData **name_to_fmt_set, 
	GTree *fmt_to_name_set);

static gint fmt_val_cmp(gconstpointer	a, gconstpointer b);

int main(int argc, char **argv)
{
	struct timespec tp;
	struct v4l2_fmtdesc fmt_dsc;
	struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
	struct bsp_v4l2_param v4l2_param;
	GData *name_to_fmt_set = NULL;
	GTree *fmt_to_name_set = NULL;
	int i, xres, yres;
	int err, vfd, pts = 0;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	struct v4l2_buffer vbuf_param;
	char pixformat[32];
	char *dev_path = NULL;
	char *ret;

	if(argc < 4)
	{
		printf("usage: bsp_v4l2_fps [dev_path] [xres] [yres]\n");
		return -1;

	}

	g_datalist_init(&name_to_fmt_set);
	fmt_to_name_set = g_tree_new(fmt_val_cmp);
	dev_path = argv[1];
	xres = atoi(argv[2]);
	yres = atoi(argv[3]);
	init_v4l2_name_fmt_set(&name_to_fmt_set, fmt_to_name_set);
	
    // Setup of the dec requests
	vfd = bsp_v4l2_open_dev(dev_path);
	v4l2_param.fps = 30;
	memset(&fmt_dsc, 0, sizeof(struct v4l2_fmtdesc));

	for(i = 0; i < V4L2_MAX_FMT; i++)
	{
		fmt_dsc.index = i;
		fmt_dsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		err = ioctl(vfd, VIDIOC_ENUM_FMT, &fmt_dsc);
		
		if (err < 0)
		{
    		break;
		}
		
		ret = NULL;
		ret = g_tree_lookup(fmt_to_name_set, fmt_dsc.pixelformat);
		
		if(NULL != ret)
		{
			printf("\n");
			printf("enter %s to select %s \n", ret, fmt_dsc.description);
			printf("\n");
		}
		
	}

renter:

	printf("\n");
	printf("please enter your pixformat choice: \n");
	printf("\n");
	scanf("%s", pixformat);
	printf("your choice: %s\n", pixformat);
	v4l2_param.pixelformat = g_datalist_get_data(&name_to_fmt_set, pixformat);
	printf("v4l2_param.pixelformat: 0x%x\n", v4l2_param.pixelformat);
	
	if(NULL == v4l2_param.pixelformat)
	{
		printf("invalid pixformat: %s\n", pixformat);
		goto renter;
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
		bsp_v4l2_get_frame(vfd, &vbuf_param);
		bsp_v4l2_put_frame_buf(vfd, &vbuf_param);
		bsp_print_fps("bsp_v4l2_fps: ", &fps, &pre_time, &curr_time);
		printf("\n");
		printf("--------------------v4l2 frame param-------------------------------------\n");
		printf("v4l2_buf_param.index : %d\n", vbuf_param.index);
		printf("v4l2_buf_param.type : %d\n", vbuf_param.type);
		printf("v4l2_buf_param.bytesused : %d\n", vbuf_param.bytesused);
		printf("v4l2_buf_param.flags : 0x%x\n", vbuf_param.flags);
		printf("v4l2_buf_param.field : %d\n", vbuf_param.field);
		printf("v4l2_buf_param.timestamp.tv_sec : %lld\n", vbuf_param.timestamp.tv_sec);
		printf("v4l2_buf_param.timestamp.tv_sec : %lld\n", vbuf_param.timestamp.tv_sec);
		printf("v4l2_buf_param.timecode.type : %d\n", vbuf_param.timecode.type);
		printf("v4l2_buf_param.timecode.flags : %d\n", vbuf_param.timecode.flags);
		printf("v4l2_buf_param.timecode.frames : %d\n", vbuf_param.timecode.frames);
		printf("v4l2_buf_param.timecode.seconds : %d\n", vbuf_param.timecode.seconds);
		printf("v4l2_buf_param.timecode.minutes : %d\n", vbuf_param.timecode.minutes);
		printf("v4l2_buf_param.timecode.hours : %d\n", vbuf_param.timecode.hours);
		printf("v4l2_buf_param.timecode.userbits[0] : %d\n", vbuf_param.timecode.userbits[0]);
		printf("v4l2_buf_param.timecode.userbits[1] : %d\n", vbuf_param.timecode.userbits[1]);
		printf("v4l2_buf_param.timecode.userbits[2] : %d\n", vbuf_param.timecode.userbits[2]);
		printf("v4l2_buf_param.timecode.userbits[3] : %d\n", vbuf_param.timecode.userbits[3]);
		printf("v4l2_buf_param.sequence : %d\n", vbuf_param.sequence);
		printf("v4l2_buf_param.memory : %d\n", vbuf_param.memory);
		printf("v4l2_buf_param.length : %d\n", vbuf_param.length);
		printf("---------------------------------------------------------\n");
		printf("\n");
		
	}
	
    return 0;
}

static void init_v4l2_name_fmt_set(GData **name_to_fmt_set, 
	GTree *fmt_to_name_set)
{
	char pixformat[32];
	
	g_datalist_set_data(name_to_fmt_set, "yuv420", V4L2_PIX_FMT_YVU420);
	g_datalist_set_data(name_to_fmt_set, "yuyv", V4L2_PIX_FMT_YUYV);
	g_datalist_set_data(name_to_fmt_set, "rgb888", V4L2_PIX_FMT_RGB24);
	g_datalist_set_data(name_to_fmt_set, "jpeg", V4L2_PIX_FMT_JPEG);
	g_datalist_set_data(name_to_fmt_set, "h264", V4L2_PIX_FMT_H264);
	g_datalist_set_data(name_to_fmt_set, "mjpeg", V4L2_PIX_FMT_MJPEG);
	g_datalist_set_data(name_to_fmt_set, "yvyu", V4L2_PIX_FMT_YVYU);
	g_datalist_set_data(name_to_fmt_set, "vyuy", V4L2_PIX_FMT_VYUY);
	g_datalist_set_data(name_to_fmt_set, "uyvy", V4L2_PIX_FMT_UYVY);
	g_datalist_set_data(name_to_fmt_set, "nv12", V4L2_PIX_FMT_NV12);
	g_datalist_set_data(name_to_fmt_set, "bgr888", V4L2_PIX_FMT_BGR24);
	g_datalist_set_data(name_to_fmt_set, "yvu420", V4L2_PIX_FMT_YVU420);
	g_datalist_set_data(name_to_fmt_set, "nv21", V4L2_PIX_FMT_NV21);
	g_datalist_set_data(name_to_fmt_set, "bgr32", V4L2_PIX_FMT_BGR32);

	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_YVU420, "yuv420");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_YUYV, "yuyv");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_RGB24, "rgb888");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_JPEG, "jpeg");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_H264, "h264");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_MJPEG, "mjpeg");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_YVYU, "yvyu");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_VYUY, "vyuy");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_UYVY, "uyvy");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_NV12, "nv12");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_BGR24, "bgr888");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_YVU420, "yvu420");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_NV21, "nv21");
	g_tree_insert(fmt_to_name_set, V4L2_PIX_FMT_BGR32, "bgr32");

}

	
static gint fmt_val_cmp(gconstpointer	a, gconstpointer b)
{
	__u32 key1 = (__u32) a;
	__u32 key2 = (__u32) b;
	
	if(key1 == key2)
		return 0;
	
	return (key1 > key2) ? 1 : -1;
}





