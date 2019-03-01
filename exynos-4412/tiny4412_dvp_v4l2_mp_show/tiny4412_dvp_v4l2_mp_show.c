/*******************************************************************************
* Copyright (C), 2000-2019,  Electronic Technology Co., Ltd.
*                
* @filename: tiny4412_dvp_v4l2_mp_show.c 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2019-3-1
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


#include <sys/time.h>
#include <stdlib.h>
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
#include "bsp_fb.h"
#include "bsp_v4l2_cap.h"

#define LIBV4L2_BUF_NR (4)
#define LIBV4L2_MAX_FMT (16)

#define CODEC_NUM_PLANES (2)
#define V4L2_OUTPUT_BUF_NR (2)
#define V4L2_CAP_BUF_NR (4)

typedef struct convert_dsc {
	int fd_cov; 
	char *cov_path;
	int bcnt;
	struct bsp_v4l2_buf cov_output_buf[V4L2_OUTPUT_BUF_NR];
	struct bsp_v4l2_buf cov_cap_buf[V4L2_CAP_BUF_NR];
	int cov_mp_flag;
} convert_dsc;

int disp_fd = 0;
static struct bsp_fb_var_attr fb_var_attr;
static struct bsp_fb_fix_attr fb_fix_attr;
static struct rgb_frame disp_frame = {
	.addr = NULL,
};

static void init_v4l2_name_fmt_set(GData **name_to_fmt_set, 
	GTree *fmt_to_name_set);

static gint fmt_val_cmp(gconstpointer	a, gconstpointer b);

static void libv4l2_print_fps(const char *fsp_dsc, long *fps, 
	long *pre_time, long *curr_time);

static int convert_to_spec_pix_fmt(struct convert_dsc *cvt,
	struct bsp_v4l2_buf *dst, __u32 dst_fmt, 
	struct bsp_v4l2_buf *src, __u32 src_fmt);



int main(int argc, char **argv)
{
	struct timespec tp;
	struct v4l2_format fmt;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmtdsc;
	struct v4l2_streamparm streamparm;
	struct v4l2_queryctrl qctrl;
	struct v4l2_buffer vbuf_param;
	struct v4l2_requestbuffers req_bufs;
	struct v4l2_input input;
	struct v4l2_subdev_mbus_code_enum mbus_code;
	struct v4l2_subdev_format subdev_format;
	struct bsp_v4l2_buf v4l2_buf[LIBV4L2_BUF_NR];
	struct bsp_v4l2_param v4l2_param;
	struct v4l2_plane mplane;
	struct pollfd fd_set[1];
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	GData *name_to_fmt_set = NULL;
	GTree *fmt_to_name_set = NULL;
	int i, xres, yres;
	int err, vfd, pts = 0;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	char pixformat[32];
	char *dev_path = NULL;
	char *ret;
	char *src_subdev = NULL, *sink_subdev = NULL;
	int fd_src, fd_sink;
	__u32 code;
	struct convert_dsc rgb_conversion;
	struct bsp_v4l2_buf rgb_frame;
	

	if(argc < 7)
	{
		printf("usage: tiny4412_dvp_v4l2_mp_show ");
		printf("[dev_path] [src_subdev] [sink_subdev] [cov_path] [xres] [yres]\n");
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

	g_datalist_init(&name_to_fmt_set);
	fmt_to_name_set = g_tree_new(fmt_val_cmp);
	dev_path = argv[1];
	src_subdev = argv[2];
	sink_subdev = argv[3];
	rgb_conversion.cov_path = argv[4];
	rgb_conversion.fd_cov = -1;
	xres = atoi(argv[5]);
	yres = atoi(argv[6]);
	init_v4l2_name_fmt_set(&name_to_fmt_set, fmt_to_name_set);
	
    // Setup of the dec requests
	vfd = open(dev_path, O_RDWR);
	
	if (vfd < 0) {
		printf("Cannot open device fd: %d\n", vfd);
		return -1;
	}

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
	
	v4l2_param.fps = 30;
	input.index = 0;
	err = ioctl(vfd, VIDIOC_S_INPUT, &input);
	
	if (err < 0) {
        printf("VIDIOC_S_INPUT failed err: %d\n", err);
 
    }
	
	memset(&fmtdsc, 0, sizeof(struct v4l2_fmtdesc));

	for(i = 0; i < LIBV4L2_MAX_FMT; i++)
	{
		fmtdsc.index = i;
		fmtdsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		err = ioctl(vfd, VIDIOC_ENUM_FMT, &fmtdsc);
		
		if (err < 0)
		{
			printf("VIDIOC_ENUM_FMT fail err %d \n", err);
    		break;
		}
		
		ret = NULL;
		ret = g_tree_lookup(fmt_to_name_set, fmtdsc.pixelformat);
		
		if(NULL != ret)
		{
			printf("\n");
			printf("enter %s to select %s \n", ret, fmtdsc.description);
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

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	fmt.fmt.pix_mp.pixelformat = v4l2_param.pixelformat;
	fmt.fmt.pix_mp.width = xres;
	fmt.fmt.pix_mp.height = yres;
	fmt.fmt.pix_mp.num_planes = 1;
	fmt.fmt.pix_mp.field = V4L2_FIELD_ANY;
	err = ioctl(vfd, VIDIOC_S_FMT, &fmt);

	if (err) 
	{
		printf("VIDIOC_S_FMT failed: %d\n", err);
		return -1;		
	}
	
	v4l2_param.xres = fmt.fmt.pix.width;
	v4l2_param.yres = fmt.fmt.pix.height;
	v4l2_param.pixelformat = fmt.fmt.pix.pixelformat;
	v4l2_param.fps = 30;
	v4l2_param.planes_num = fmt.fmt.pix_mp.num_planes;
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);

	fd_src = open(src_subdev, O_RDWR);

	if(fd_src < 0)
	{
		return fd_src;
	}

	fd_sink = open(sink_subdev, O_RDWR);

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
	

	/* init buf */
	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
	req_bufs.count = LIBV4L2_BUF_NR;
	req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	req_bufs.memory = V4L2_MEMORY_MMAP;
	err = ioctl(vfd, VIDIOC_REQBUFS, &req_bufs);
		
	if (err) 
	{
		printf("VIDIOC_REQBUFS fail err: %d\n", err);
		return -1;		  
	}
		
	for(i = 0; i < LIBV4L2_BUF_NR; i++)
	{
		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.index = i;
		vbuf_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = &mplane;
		vbuf_param.length = v4l2_param.planes_num;
		err = ioctl(vfd, VIDIOC_QUERYBUF, &vbuf_param);
		
		if (err) 
		{
			printf("VIDIOC_QUERYBUF fail err: %d\n", err);
			return -1;
		}

		v4l2_buf[i].xres = v4l2_param.xres;
		v4l2_buf[i].yres = v4l2_param.yres;
		v4l2_buf[i].planes_num = v4l2_param.planes_num;
		v4l2_buf[i].bytes[0] = vbuf_param.m.planes->length;
		v4l2_buf[i].addr[0] = mmap(0 , v4l2_buf[i].bytes[0], 
								PROT_READ, MAP_SHARED, vfd, 
								vbuf_param.m.planes->m.mem_offset);
		err = ioctl(vfd, VIDIOC_QBUF, &vbuf_param);
				
		if (err) 
		{
			printf("VIDIOC_QBUF fail err: %d\n", err);
			return -1;
		}
	
	}


	err = ioctl(vfd, VIDIOC_STREAMON, &video_type);
	
	if (err < 0) 
    {
		printf("VIDIOC_STREAMON fail err: %d\n", err);
		return -1;
    }

	while(++pts <= 1000)
	{
		fd_set[0].fd     = vfd;
    	fd_set[0].events = POLLIN;
    	err = poll(fd_set, 1, -1);

		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
   		vbuf_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
   		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = &mplane;
		vbuf_param.length = v4l2_param.planes_num;
    	err = ioctl(vfd, VIDIOC_DQBUF, &vbuf_param);
		
		if (err < 0) 
		{
			printf("VIDIOC_DQBUF fail err: %d\n", err);
		}
			
		// ioctl(vfd, VIDIOC_LOG_STATUS, NULL);
	
		libv4l2_print_fps("bsp_v4l2_fps: ", &fps, &pre_time, &curr_time);

		disp_frame.xres = xres;
		disp_frame.yres = yres;
		disp_frame.bits_per_pixel = fb_var_attr.bits_per_pixel;
		disp_frame.bytes_per_line = xres * (disp_frame.bits_per_pixel >> 3);
		disp_frame.bytes = disp_frame.bytes_per_line * yres;
		
		if(NULL == disp_frame.addr)
		{
			disp_frame.addr = malloc(disp_frame.bytes);
		}

		rgb_frame.planes_num = 1;
		rgb_frame.xres = disp_frame.xres;
		rgb_frame.yres = disp_frame.yres;
		rgb_frame.bytes[0] = disp_frame.bytes;
		rgb_frame.addr[0] = disp_frame.addr;
		ret = convert_to_spec_pix_fmt(&rgb_conversion, 
					&rgb_frame, V4L2_PIX_FMT_BGR32,
					&v4l2_buf[vbuf_param.index], v4l2_param.pixelformat);
		// memcpy(disp_frame.addr, v4l2_buf[vbuf_param.index].addr, v4l2_buf[vbuf_param.index].bytes);
		bsp_fb_flush(disp_fd, &fb_var_attr, &fb_fix_attr, &disp_frame);
		err = ioctl(vfd, VIDIOC_QBUF, &vbuf_param);
	}

	close(fd_src);
	close(fd_sink);
	close(vfd);
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

static void libv4l2_print_fps(const char *fsp_dsc, long *fps, 
	long *pre_time, long *curr_time)
{
	struct timespec tp;

	clock_gettime(CLOCK_MONOTONIC, &tp);
	*curr_time = tp.tv_sec;
	(*fps)++;
		
	if(((*curr_time) - (*pre_time)) >= 1)
	{	
		printf("%s fps: %d \n", fsp_dsc, *fps);
		*pre_time = *curr_time;
		*fps = 0;
	}
	
}
	
static int convert_to_spec_pix_fmt(struct convert_dsc *cvt,
	struct bsp_v4l2_buf *dst, __u32 dst_fmt, 
	struct bsp_v4l2_buf *src, __u32 src_fmt)
{
	struct bsp_v4l2_param cov_param;
	struct v4l2_buffer vbuf_param;
	struct v4l2_plane mplanes[CODEC_NUM_PLANES];
	int ret, i, j = 0;
	struct pollfd fd_set[1];
	
	if(cvt->fd_cov < 0)
	{
		cvt->fd_cov = bsp_v4l2_open_dev(cvt->cov_path, &cvt->cov_mp_flag);
		cov_param.pixelformat = src_fmt;
		cov_param.xres = src->xres;
		cov_param.yres = src->yres;
		cov_param.planes_num = src->planes_num;
		
		for(i = 0; i < cov_param.planes_num; i++)
		{
			cov_param.req_buf_size[i] = src->bytes[i];
		}
		
		bsp_v4l2_try_setup(cvt->fd_cov, &cov_param, (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));
		cvt->bcnt = bsp_v4l2_req_buf(cvt->fd_cov, cvt->cov_output_buf, 1, 
				(cvt->cov_mp_flag ? 
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT), 
				cov_param.planes_num);
		
		if(cvt->bcnt < 0)
		{
			printf("bsp_v4l2_req_buf failed err: %d line: %d\n", cvt->bcnt, __LINE__);
			return -1;
		}

	 	cov_param.pixelformat = dst_fmt;
		cov_param.xres = dst->xres;
		cov_param.yres = dst->yres;
		cov_param.planes_num = dst->planes_num;
		for(i = 0; i < cov_param.planes_num; i++)
		{
			cov_param.req_buf_size[i] = dst->bytes[i];
		}
		bsp_v4l2_try_setup(cvt->fd_cov, &cov_param, (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
		cvt->bcnt = bsp_v4l2_req_buf(cvt->fd_cov, cvt->cov_cap_buf, 1, 
				(cvt->cov_mp_flag ? 
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE), 
				cov_param.planes_num);
		
		if(cvt->bcnt < 0)
		{
			printf("bsp_v4l2_req_buf failed err: %d line: %d\n", cvt->bcnt, __LINE__);
			return -1;
		}

		for(i = 0; i < src->planes_num; i++)
		{
			memcpy(cvt->cov_output_buf[0].addr[i], src->addr[i], src->bytes[i]);
		}

		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.index = 0;
		vbuf_param.type = (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = mplanes;
		vbuf_param.length = src->planes_num;
		ret = ioctl(cvt->fd_cov, VIDIOC_QUERYBUF, &vbuf_param);
			
		if (ret) 
		{
			printf("VIDIOC_QUERYBUF err: %d line: %d\n", ret, __LINE__);
			return -1;
    	}
		
		for(j = 0; j < src->planes_num; j++)
		{
			vbuf_param.m.planes[j].bytesused = src->bytes[j];
		}

		ret = bsp_v4l2_put_frame_buf(cvt->fd_cov, &vbuf_param);
		
		if(ret < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", ret, __LINE__);
			return -1;
		}
		
		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.index = 0;
		vbuf_param.type = (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = mplanes;
		vbuf_param.length = dst->planes_num;
		ret = ioctl(cvt->fd_cov, VIDIOC_QUERYBUF, &vbuf_param);
			
		if (ret) 
		{
			printf("VIDIOC_QUERYBUF err: %d line: %d\n", ret, __LINE__);
			return -1;
    	}
		
		for(j = 0; j < 1; j++)
		{
			vbuf_param.m.planes[j].bytesused = cvt->cov_cap_buf[0].bytes[j];
		}
		
		ret = bsp_v4l2_put_frame_buf(cvt->fd_cov, &vbuf_param);
		
		if(ret < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", ret, __LINE__);
			return -1;
		}

		ret = bsp_v4l2_stream_on(cvt->fd_cov, (cvt->cov_mp_flag  ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));

		if(ret < 0)
		{
			printf("bsp_v4l2_stream_on failed err: %d line: %d\n", ret, __LINE__);
			return -1;
		}

		ret = bsp_v4l2_stream_on(cvt->fd_cov, (cvt->cov_mp_flag  ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));

		if(ret < 0)
		{
			printf("bsp_v4l2_stream_on failed err: %d line: %d\n", ret, __LINE__);
			return -1;
		}

		fd_set[0].fd     = cvt->fd_cov;
    	fd_set[0].events = POLLIN;
    	ret = poll(fd_set, 1, -1);

		if(ret < 0)
		{
			printf("[%s]:[%d] poll error: %d\n", __FUNCTION__, __LINE__, ret);
		}

		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.type = (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = mplanes;
		vbuf_param.length = src->planes_num;
		ret = bsp_v4l2_get_frame_buf(cvt->fd_cov, &vbuf_param, 
				(cvt->cov_mp_flag ? 
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT),
				src->planes_num);
		
		if(ret < 0)
		{
			printf("bsp_v4l2_get_frame_buf err: %d, line: %d\n", ret, __LINE__);
		}

		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.type = (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = mplanes;
		vbuf_param.length = dst->planes_num;
		ret = bsp_v4l2_get_frame_buf(cvt->fd_cov, &vbuf_param, (cvt->cov_mp_flag ? 
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
				dst->planes_num);
		
		if(ret < 0)
		{
			printf("bsp_v4l2_get_frame_buf err: %d, line: %d\n", ret, __LINE__);
		}

		
		for(i = 0; i < dst->planes_num; i++)
		{
			memcpy(dst->addr[i], cvt->cov_cap_buf[0].addr[i], dst->bytes[i]);
		}
		
		return 0;
	}
	
	for(i = 0; i < src->planes_num; i++)
	{
		memcpy(cvt->cov_output_buf[0].addr[i], src->addr[i], src->bytes[i]);
	}

	memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
	vbuf_param.index = 0;
	vbuf_param.type = (cvt->cov_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
	vbuf_param.memory = V4L2_MEMORY_MMAP;
	vbuf_param.m.planes = mplanes;
	vbuf_param.length = src->planes_num;
	ret = ioctl(cvt->fd_cov, VIDIOC_QUERYBUF, &vbuf_param);
		
	if (ret) 
	{
		printf("VIDIOC_QUERYBUF err: %d line: %d\n", ret, __LINE__);
		return -1;
	}
	
	for(j = 0; j < src->planes_num; j++)
	{
		vbuf_param.m.planes[j].bytesused = src->bytes[j];
	}

	ret = bsp_v4l2_put_frame_buf(cvt->fd_cov, &vbuf_param);

	if(ret < 0)
	{
		printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", ret, __LINE__);
		return -1;
	}

	memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
	vbuf_param.index = 0;
	vbuf_param.type = (cvt->cov_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	vbuf_param.memory = V4L2_MEMORY_MMAP;
	vbuf_param.m.planes = mplanes;
	vbuf_param.length = dst->planes_num;
	ret = ioctl(cvt->fd_cov, VIDIOC_QUERYBUF, &vbuf_param);
		
	if (ret) 
	{
		printf("VIDIOC_QUERYBUF err: %d line: %d\n", ret, __LINE__);
		return -1;
	}
	
	for(j = 0; j < 1; j++)
	{
		vbuf_param.m.planes[j].bytesused = cvt->cov_cap_buf[0].bytes[j];
	}
	
	ret = bsp_v4l2_put_frame_buf(cvt->fd_cov, &vbuf_param);
	
	if(ret < 0)
	{
		printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", ret, __LINE__);
		return -1;
	}
	
	fd_set[0].fd	 = cvt->fd_cov;
	fd_set[0].events = POLLIN;
	ret = poll(fd_set, 1, -1);
	
	if(ret < 0)
	{
		printf("[%s]:[%d] poll error: %d\n", __FUNCTION__, __LINE__, ret);
	}
	
	memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
	vbuf_param.type = (cvt->cov_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
	vbuf_param.memory = V4L2_MEMORY_MMAP;
	vbuf_param.m.planes = mplanes;
	vbuf_param.length = src->planes_num;
	ret = bsp_v4l2_get_frame_buf(cvt->fd_cov, &vbuf_param, (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT),
			src->planes_num);
	
	if(ret < 0)
	{
		printf("bsp_v4l2_get_frame_buf err: %d, line: %d\n", ret, __LINE__);
	}
	
	memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
	vbuf_param.type = (cvt->cov_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	vbuf_param.memory = V4L2_MEMORY_MMAP;
	vbuf_param.m.planes = mplanes;
	vbuf_param.length = dst->planes_num;
	ret = bsp_v4l2_get_frame_buf(cvt->fd_cov, &vbuf_param, (cvt->cov_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
			dst->planes_num);
	
	if(ret < 0)
	{
		printf("bsp_v4l2_get_frame_buf err: %d, line: %d\n", ret, __LINE__);
	}

	for(i = 0; i < dst->planes_num; i++)
	{
		memcpy(dst->addr[i], cvt->cov_cap_buf[0].addr[i], dst->bytes[i]);
	}
	
	return ret;

}

