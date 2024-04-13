/*******************************************************************************
* Copyright (C), 2000-2019,  Electronic Technology Co., Ltd.
*                
* @filename: tiny4412_hw_jpeg_dec.c 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2019-1-14    
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

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <gmodule.h>
#include "jpeg_k44_hal.h"
#include "bsp_fb.h"
#include "bsp_v4l2_cap.h"

#define LIBV4L2_BUF_NR (4)
#define LIBV4L2_MAX_FMT (16)

typedef struct libv4l2_param {
	__u32 xres;
	__u32 yres;
	__u32 pixelformat;
	__u32 fps;
} libv4l2_param;

typedef struct libv4l2_cap_buf {
	int bytes;
	char *addr;
} libv4l2_cap_buf;

int disp_fd = 0;
static struct bsp_fb_var_attr fb_var_attr;
static struct bsp_fb_fix_attr fb_fix_attr;
static struct rgb_frame disp_frame = {
	.addr = NULL,
};

int main(int argc, char **argv)
{
	struct v4l2_format fmt;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmtdsc;
	struct v4l2_buffer vbuf_param;
	struct v4l2_requestbuffers req_bufs;
	struct jpeg_buf in_buf;
	struct jpeg_buf out_buf;
	struct libv4l2_param v4l2_param;
	struct libv4l2_cap_buf v4l2_buf[LIBV4L2_BUF_NR];
	struct pollfd fd_set[1];
	int buf_mp_flag = 0;
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int xres;
	int yres;
	int scaled_xres;//640;
	int scaled_yres;//480;
	int i, err, vfd, pts = 0;
	int fps = 0;
	char pixformat[32];
	char *dev_path, *dec_path;
	char *ret;
	char *src_path;
	struct stat image_stat;
	char *image_addr;
	int fd_src;
	int rd_size, total_size= 0;
	struct jpeg_config dec_config;
	
	if(argc < 5)
	{
		printf("usage: tiny4412_hw_jpeg_dec [decode_path] [source_path] [xres] [yres]\n");
		return -1;
	}

	disp_fd = bsp_fb_open_dev("/dev/fb0", &fb_var_attr, &fb_fix_attr);
	fb_var_attr.red_offset = 16;
	fb_var_attr.green_offset = 8;
	fb_var_attr.blue_offset = 0;
	fb_var_attr.transp_offset = 24;
	err = bsp_fb_try_setup(disp_fd, &fb_var_attr);
	
	if(err < 0)
	{
		fprintf(stderr, "could not bsp_fb_try_setup\n");
	}

	dec_path = argv[1];
	src_path = argv[2];
	xres = atoi(argv[3]);
	yres = atoi(argv[4]);
	scaled_xres = xres;
	scaled_yres = yres;
    // Setup of the dec requests
	vfd = jpeghal_dec_init(dec_path);

	err = ioctl(vfd, VIDIOC_QUERYCAP, &cap);

	if(err < 0)
	{	
		printf("VIDIOC_QUERYCAP failed: %d\n", err);
		return -1;
	}

	printf("[%s]: v4l2_cap.driver: %s \n", dev_path, cap.driver);
	printf("[%s]: v4l2_cap.card: %s \n", dev_path, cap.card);
	printf("[%s]: v4l2_cap.bus_info: %s \n", dev_path, cap.bus_info);
	printf("[%s]: v4l2_cap.version: 0x%x \n", dev_path, cap.version);
	printf("[%s]: v4l2_cap.capabilities: 0x%x \n", dev_path, cap.capabilities);	
	memset(&fmtdsc, 0, sizeof(struct v4l2_fmtdesc));

	for(i = 0; i < LIBV4L2_MAX_FMT; i++)
	{
		fmtdsc.index = i;
		fmtdsc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		err = ioctl(vfd, VIDIOC_ENUM_FMT, &fmtdsc);
		
		if (err < 0)
		{
			printf("VIDIOC_ENUM_FMT fail err %d \n", err);
    		break;
		}

		printf("\n");
		printf("------------V4L2_BUF_TYPE_VIDEO_OUTPUT--------\n");
		printf("[%s]: fmt_dsc.index: %d \n", dev_path, fmtdsc.index);
		printf("[%s]: fmt_dsc.type: 0x%x \n", dev_path, fmtdsc.type);
		printf("[%s]: fmt_dsc.flags: 0x%x \n", dev_path, fmtdsc.flags);
		printf("[%s]: fmt_dsc.description: %s \n", dev_path, fmtdsc.description);
		printf("[%s]: fmt_dsc.pixelformat: 0x%x \n", dev_path, fmtdsc.pixelformat);
		printf("\n");
		
	}

	for(i = 0; i < LIBV4L2_MAX_FMT; i++)
	{
		fmtdsc.index = i;
		fmtdsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		err = ioctl(vfd, VIDIOC_ENUM_FMT, &fmtdsc);
		
		if (err < 0)
		{
			printf("VIDIOC_ENUM_FMT fail err %d \n", err);
    		break;
		}

		printf("\n");
		printf("------------V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE--------\n");
		printf("[%s]: fmt_dsc.index: %d \n", dev_path, fmtdsc.index);
		printf("[%s]: fmt_dsc.type: 0x%x \n", dev_path, fmtdsc.type);
		printf("[%s]: fmt_dsc.flags: 0x%x \n", dev_path, fmtdsc.flags);
		printf("[%s]: fmt_dsc.description: %s \n", dev_path, fmtdsc.description);
		printf("[%s]: fmt_dsc.pixelformat: 0x%x \n", dev_path, fmtdsc.pixelformat);
		printf("\n");
	}

	printf("src_path: %s\n", src_path);
	fd_src = open(src_path, O_RDONLY);
	
	if(fd_src < 0) 
	{
		printf("can't open this image file line: %d\n", __LINE__);
		return -1;
	}

	if(fstat(fd_src, &image_stat))
	{
		printf("can't get image stat\ line: %d\n", __LINE__);
		return -1;
	}

	image_addr = mmap(NULL, image_stat.st_size, PROT_READ, 
					MAP_SHARED, fd_src, 0);
	
	if(image_addr == MAP_FAILED)
	{
		printf("can't mmap this file line: %d\n", __LINE__);
		return -1;
	}		

	dec_config.mode = JPEG_DECODE;
	dec_config.width = xres;
	dec_config.height = yres;
	dec_config.num_planes = 1;
	dec_config.scaled_width = scaled_xres;
	dec_config.scaled_height = scaled_yres;
	dec_config.sizeJpeg = image_stat.st_size;
	dec_config.pix.dec_fmt.in_fmt = V4L2_PIX_FMT_JPEG;
	dec_config.pix.dec_fmt.out_fmt = V4L2_PIX_FMT_RGB32;
	jpeghal_dec_setconfig(vfd, &dec_config);
	jpeghal_dec_getconfig(vfd, &dec_config);
	v4l2_param.xres = dec_config.scaled_width;
	v4l2_param.yres = dec_config.scaled_height;
	v4l2_param.pixelformat = dec_config.pix.dec_fmt.out_fmt;
	printf("v4l2_param.fps: %d\n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x\n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d\n", v4l2_param.xres);
	printf("v4l2_param.yres: %d\n", v4l2_param.yres);
	in_buf.memory = V4L2_MEMORY_MMAP;
	in_buf.num_planes = 1;
	jpeghal_set_inbuf(vfd, &in_buf);
	out_buf.memory = V4L2_MEMORY_MMAP;
	out_buf.num_planes = 1;
	jpeghal_set_outbuf(vfd, &out_buf);
	printf("image_stat.st_size: %d, in_buf.length[0]: %d\n", 
			image_stat.st_size, in_buf.length[0]);
	memcpy(in_buf.start[0], image_addr, image_stat.st_size);
	jpeghal_dec_exe(vfd, &in_buf, &out_buf);
	// ioctl(vfd, VIDIOC_LOG_STATUS, NULL);
	disp_frame.xres = v4l2_param.xres;
	disp_frame.yres = v4l2_param.yres;
	disp_frame.bits_per_pixel = fb_var_attr.bits_per_pixel;
	disp_frame.bytes_per_line = v4l2_param.xres * (disp_frame.bits_per_pixel >> 3);
	disp_frame.bytes = disp_frame.bytes_per_line * v4l2_param.yres;
		
	if(NULL == disp_frame.addr)
	{
		disp_frame.addr = malloc(disp_frame.bytes);
	}

	memcpy(disp_frame.addr, out_buf.start[0], out_buf.length[0]);
	bsp_fb_flush(disp_fd, &fb_var_attr, &fb_fix_attr, &disp_frame);	
	close(fd_src);
	jpeghal_deinit(vfd, &in_buf, &out_buf);
	printf("jpeg dec finish return 0\n");
    return 0;
}

