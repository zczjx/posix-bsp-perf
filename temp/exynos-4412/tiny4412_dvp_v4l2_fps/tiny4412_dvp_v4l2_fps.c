/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: tiny4412_dvp_v4l2_fps.c 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2018-12-23    
*                
* @brief:          
*                  
*                  
* @details:        
*                 
*    
*    
* @comment           
*******************************************************************************/

#include "bsp_v4l2_cap.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <gmodule.h>

#define V4L2_BUF_NR (4)
#define V4L2_MAX_FMT (16)

static void init_v4l2_name_fmt_set(GData **name_to_fmt_set, 
	GTree *fmt_to_name_set);

static gint fmt_val_cmp(gconstpointer	a, gconstpointer b);

int main(int argc, char **argv)
{
	struct timespec tp;
	struct v4l2_fmtdesc fmt_dsc;
	struct v4l2_capability v4l2_cap;
	struct bsp_v4l2_buf v4l2_buf[V4L2_BUF_NR];
	struct bsp_v4l2_param v4l2_param;
	struct v4l2_input input;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	struct v4l2_subdev_format subdev_format;
	struct v4l2_buffer vbuf_param;
	struct v4l2_plane mplane;
	struct pollfd fd_set[1];
	GData *name_to_fmt_set = NULL;
	GTree *fmt_to_name_set = NULL;
	int i, xres, yres;
	int err, buf_cnt, vfd, pts = 0;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	int buf_mp_flag = 0;
	char pixformat[32];
	char *dev_path = NULL;
	char *src_subdev = NULL, *sink_subdev = NULL;
	char *ret;
	int fd_src, fd_sink;
	__u32 code;
	char *print_frame_info = NULL;

	if(argc < 7)
	{
		printf("usage: tiny4412_dvp_v4l2_fps ");
		printf("[dev_path] [src_subdev] [sink_subdev] [xres] [yres] [print_frame_info y|n]\n");
		return -1;
	}

	g_datalist_init(&name_to_fmt_set);
	fmt_to_name_set = g_tree_new(fmt_val_cmp);
	dev_path = argv[1];
	src_subdev = argv[2];
	sink_subdev = argv[3];
	xres = atoi(argv[4]);
	yres = atoi(argv[5]);
	print_frame_info = argv[6];
	init_v4l2_name_fmt_set(&name_to_fmt_set, fmt_to_name_set);
	
    // Setup of the dec requests
	vfd = bsp_v4l2_open_dev(dev_path, &buf_mp_flag);
	v4l2_param.fps = 30;
	input.index = 0;
	err = ioctl(vfd, VIDIOC_S_INPUT, &input);
	
	if (err < 0) {
        printf("VIDIOC_S_INPUT failed err: %d\n", err);
    }
	
	memset(&fmt_dsc, 0, sizeof(struct v4l2_fmtdesc));

	for(i = 0; i < V4L2_MAX_FMT; i++)
	{
		fmt_dsc.index = i;
		fmt_dsc.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
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
	v4l2_param.planes_num = 1;
	v4l2_param.req_buf_size[0] = 0;
	bsp_v4l2_try_setup(vfd, &v4l2_param, (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);
	fd_src = bsp_v4l2_subdev_open(src_subdev);

	if(fd_src < 0)
	{
		return fd_src;
	}

	fd_sink = bsp_v4l2_subdev_open(sink_subdev);

	if(fd_sink < 0)
	{
		return fd_sink;
	}

	memset(&mbus_code, 0, sizeof(struct v4l2_subdev_mbus_code_enum));

	printf("--------enum v4l2 subdev source format-------------\n");
	for(i = 0; i < 32; i++)
	{
		mbus_code.index = i;
		err = ioctl(fd_src, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_code);
	
		if (err < 0)
    	{
			break;
		}

	
		printf("[%s]: mbus_code.pad: %d \n", dev_path, mbus_code.pad);
		printf("[%s]: mbus_code.index: %d \n", dev_path, mbus_code.index);
		printf("[%s]: mbus_code.code: 0x%x \n", dev_path, mbus_code.code);
		printf("\n");
	}
	printf("--------------------------------------\n");
	printf("\n");
	printf("--------enum v4l2 subdev sink format--------------\n");

	for(i = 0; i < 32; i++)
	{
		mbus_code.index = i;
		err = ioctl(fd_sink, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_code);
	
		if (err < 0)
    	{
			break;
		}
		
		printf("[%s]: mbus_code.pad: %d \n", dev_path, mbus_code.pad);
		printf("[%s]: mbus_code.index: %d \n", dev_path, mbus_code.index);
		printf("[%s]: mbus_code.code: 0x%x \n", dev_path, mbus_code.code);
		printf("\n");
	}

	printf("\n");
	printf("please enter your mbus_code choice:  \n");
	scanf("%x", &code);
	printf("\n");
	printf("your input mbus_code is 0x%x\n", code);

	memset(&subdev_format, 0, sizeof(struct v4l2_subdev_format));
	subdev_format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	subdev_format.pad  = 0;
	subdev_format.format.width = xres;
	subdev_format.format.height = yres;
	subdev_format.format.code = code;

	err = ioctl(fd_sink, VIDIOC_SUBDEV_S_FMT, &subdev_format);
	
	if (err < 0)
	{
		printf("fd_sink VIDIOC_SUBDEV_S_FMT failed err: %d\n", err);
		return -1;
	}

	memset(&subdev_format, 0, sizeof(struct v4l2_subdev_format));
	subdev_format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	subdev_format.pad  = 0;
	subdev_format.format.width = xres;
	subdev_format.format.height = yres;
	subdev_format.format.code = code;
	err = ioctl(fd_src, VIDIOC_SUBDEV_S_FMT, &subdev_format);
	
	if (err < 0)
	{
		printf("fd_src VIDIOC_SUBDEV_S_FMT failed err: %d\n", err);
		return -1;
	}

	memset(&subdev_format, 0, sizeof(struct v4l2_subdev_format));
	subdev_format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	err = ioctl(fd_src, VIDIOC_SUBDEV_G_FMT, &subdev_format);

	printf("[%s]: subdev_format.which: %d \n", src_subdev, subdev_format.which);
	printf("[%s]: subdev_format.pad: %d \n", src_subdev, subdev_format.pad);
	printf("[%s]: subdev_format.format.width: %d \n", 
		src_subdev, subdev_format.format.width);
	printf("[%s]: subdev_format.format.height: %d \n", 
		src_subdev, subdev_format.format.height);
	printf("[%s]: subdev_format.format.code: 0x%x \n", 
		src_subdev, subdev_format.format.code);
	printf("[%s]: subdev_format.format.field: %d \n", 
		src_subdev, subdev_format.format.field);
	printf("[%s]: subdev_format.format.colorspace: %d \n", 
		src_subdev, subdev_format.format.colorspace);
	printf("[%s]: subdev_format.format.ycbcr_enc: %d \n", 
		src_subdev, subdev_format.format.ycbcr_enc);
	printf("[%s]: subdev_format.format.quantization: %d \n", 
		src_subdev, subdev_format.format.quantization);

	memset(&subdev_format, 0, sizeof(struct v4l2_subdev_format));
	subdev_format.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	err = ioctl(fd_sink, VIDIOC_SUBDEV_G_FMT, &subdev_format);
		
	printf("[%s]: subdev_format.which: %d \n", sink_subdev, subdev_format.which);
	printf("[%s]: subdev_format.pad: %d \n", sink_subdev, subdev_format.pad);
	printf("[%s]: subdev_format.format.width: %d \n", 
		sink_subdev, subdev_format.format.width);
	printf("[%s]: subdev_format.format.height: %d \n", 
		sink_subdev, subdev_format.format.height);
	printf("[%s]: subdev_format.format.code: 0x%x \n", 
		sink_subdev, subdev_format.format.code);
	printf("[%s]: subdev_format.format.field: %d \n", 
		sink_subdev, subdev_format.format.field);
	printf("[%s]: subdev_format.format.colorspace: %d \n", 
		sink_subdev, subdev_format.format.colorspace);
	printf("[%s]: subdev_format.format.ycbcr_enc: %d \n", 
		sink_subdev, subdev_format.format.ycbcr_enc);
	printf("[%s]: subdev_format.format.quantization: %d \n", 
		sink_subdev, subdev_format.format.quantization);
	
	buf_cnt = bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR, (buf_mp_flag ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
			(buf_mp_flag ? 1 : 0));

	if(buf_cnt < 0)
	{
		printf("bsp_v4l2_req_buf failed err: %d\n", buf_cnt);
		return -1;
	}

	for(i = 0; i < buf_cnt; i++)
	{
		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.index = i;
		vbuf_param.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = &mplane;
		vbuf_param.length = 1;
		err = bsp_v4l2_put_frame_buf(vfd, &vbuf_param);
		
		if(err < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", err, __LINE__);
			return -1;
		}
	
	}
	
	err = bsp_v4l2_stream_on(vfd, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));

	if(err < 0)
	{
		printf("bsp_v4l2_stream_on failed err: %d\n", err);
		return -1;

	}

	while(++pts <= 1000)
	{
		fd_set[0].fd = vfd;
		fd_set[0].events = POLLIN;
		err = poll(fd_set, 1, -1);
	
		err = bsp_v4l2_get_frame_buf(vfd, &vbuf_param, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
			(buf_mp_flag ? 1 : 0));
		err = bsp_v4l2_put_frame_buf(vfd, &vbuf_param);	
		bsp_print_fps("bsp_v4l2_fps: ", &fps, &pre_time, &curr_time);
		
		if('y' == print_frame_info[0]
		|| 'Y' == print_frame_info[0])
		{
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
		
	}

	bsp_v4l2_stream_off(vfd, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	close(vfd);
	close(fd_src);
	close(fd_sink);
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

