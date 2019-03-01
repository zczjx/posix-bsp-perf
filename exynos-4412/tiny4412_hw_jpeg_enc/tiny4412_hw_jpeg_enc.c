/*******************************************************************************
* Copyright (C), 2000-2019,  Electronic Technology Co., Ltd.
*                
* @filename: tiny4412_hw_jpeg_enc.c 
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
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <poll.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <gmodule.h>

#include "jpeg_k44_hal.h"
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


static int buf_mp_flag = 0;
static int disp_fd = 0;
static pthread_mutex_t snap_lock = PTHREAD_MUTEX_INITIALIZER;
static int snapshot_run = 0;
static struct bsp_v4l2_param v4l2_param;
static struct bsp_v4l2_buf v4l2_buf[LIBV4L2_BUF_NR];
static struct v4l2_buffer vbuf_param;
static int xres = 640;
static int yres = 480;
static struct bsp_fb_var_attr fb_var_attr;
static struct bsp_fb_fix_attr fb_fix_attr;
static struct rgb_frame disp_frame = {
	.addr = NULL,
};

static int convert_to_spec_pix_fmt(struct convert_dsc *cvt,
	struct bsp_v4l2_buf *dst, __u32 dst_fmt, 
	struct bsp_v4l2_buf *src, __u32 src_fmt);

static void *snapshot_thread(void *arg);

int main(int argc, char **argv)
{
	struct v4l2_format fmt;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmtdsc;
	struct v4l2_requestbuffers req_bufs;
	struct pollfd fd_set[1];
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i, buf_cnt, err, pts = 0;
	int fps = 0;
	char pixformat[32];
	char *ret;
	char *src_path, *enc_path;
	struct stat image_stat;
	int fd_src;
	int rd_size, total_size= 0;
	pthread_t snap_tid;
	struct convert_dsc rgb_conversion;
	struct bsp_v4l2_buf rgb_frame;
	
	if(argc < 4)
	{
		printf("usage: tiny4412_hw_jpeg_enc [source_path] [encode_path] [cov_path]\n");
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
	
	src_path = argv[1];
	enc_path = argv[2];
	rgb_conversion.cov_path = argv[3];
	rgb_conversion.fd_cov = -1;
	fd_src = bsp_v4l2_open_dev(src_path, &buf_mp_flag);
	v4l2_param.pixelformat = V4L2_PIX_FMT_YUYV;
	v4l2_param.xres = xres;
	v4l2_param.yres = yres;
	bsp_v4l2_try_setup(fd_src, &v4l2_param, (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	printf("v4l2_param.fps: %d\n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x\n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d\n", v4l2_param.xres);
	printf("v4l2_param.yres: %d\n", v4l2_param.yres);
	buf_cnt = bsp_v4l2_req_buf(fd_src, v4l2_buf, LIBV4L2_BUF_NR, (buf_mp_flag ?
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
			(buf_mp_flag ? 1 : 0));

	if(buf_cnt < 0)
	{
		printf("bsp_v4l2_req_buf failed err: %d\n", err);
		return -1;
	}

	for(i = 0; i < buf_cnt; i++)
	{
		vbuf_param.index = i;
		vbuf_param.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		err = bsp_v4l2_put_frame_buf(fd_src, &vbuf_param);
		if(err < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", err, __LINE__);
			return -1;
		}
	}

	err = bsp_v4l2_stream_on(fd_src, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));

	if(err < 0)
	{
		printf("bsp_v4l2_stream_on failed err: %d\n", err);
		return -1;
	}

	snapshot_run = 1;
	err = pthread_create(&snap_tid, NULL, snapshot_thread, enc_path);
	
	while(++pts <= 1000)
	{
		struct pollfd fd_set[1];

		fd_set[0].fd = fd_src;
		fd_set[0].events = POLLIN;
		err = poll(fd_set, 1, -1);
		err = bsp_v4l2_get_frame_buf(fd_src, &vbuf_param, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
			(buf_mp_flag ? 1 : 0));
		disp_frame.xres = xres;
		disp_frame.yres = yres;
		disp_frame.bits_per_pixel = fb_var_attr.bits_per_pixel;
		disp_frame.bytes_per_line = xres * (disp_frame.bits_per_pixel >> 3);
		disp_frame.bytes = disp_frame.bytes_per_line * yres;
		
		if(NULL == disp_frame.addr)
		{
			disp_frame.addr = malloc(disp_frame.bytes);
		}

		if(buf_mp_flag)
		{
			for(i = 0; i < 1; i++)
			{
				v4l2_buf[vbuf_param.index].bytes[i] = 
					vbuf_param.m.planes[i].bytesused;
			}
		}
		else
		{
			v4l2_buf[vbuf_param.index].bytes[0] = vbuf_param.bytesused;
		}

		v4l2_buf[vbuf_param.index].planes_num = 1;
		v4l2_buf[vbuf_param.index].xres = v4l2_param.xres;
		v4l2_buf[vbuf_param.index].yres = v4l2_param.yres;

		rgb_frame.planes_num = 1;
		rgb_frame.xres = disp_frame.xres;
		rgb_frame.yres = disp_frame.yres;
		rgb_frame.bytes[0] = disp_frame.bytes;
		rgb_frame.addr[0] = disp_frame.addr;

		ret = convert_to_spec_pix_fmt(&rgb_conversion, 
					&rgb_frame, V4L2_PIX_FMT_BGR32,
					&v4l2_buf[vbuf_param.index], V4L2_PIX_FMT_YUYV);
		pthread_mutex_lock(&snap_lock);
		err = bsp_v4l2_put_frame_buf(fd_src, &vbuf_param);
		pthread_mutex_unlock(&snap_lock);
		bsp_fb_flush(disp_fd, &fb_var_attr, &fb_fix_attr, &disp_frame);	
	}
	
	snapshot_run = 0;
	bsp_v4l2_stream_off(fd_src, 
		(buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	close(fd_src);
	printf("jpeg enc finish return 0\n");
    return 0;
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


static void *snapshot_thread(void *arg)
{
	int vfd = -1;
	struct timespec tp;
	struct jpeg_buf in_buf;
	struct jpeg_buf out_buf;
	struct jpeg_config enc_config;
	int fd_img = -1;
	char *image_addr;
	char cmd;
	char img_name[64];
	char *enc_path = (char *) arg;
	
	while(1 == snapshot_run)
	{
		printf("enter 's' to do snapshot: \n");
		scanf("%c", &cmd);
		
		if('s' == cmd)
		{
			/* it is a workaround solution that open/close jpeg encode
			   in each picture to WAR the linux-4.x encode driver stuck
			   the system when continuous qbuf/dqbuf
			*/

			// Setup of the dec requests
			vfd = jpeghal_enc_init(enc_path);
			enc_config.mode = JPEG_ENCODE;
			enc_config.enc_qual = QUALITY_LEVEL_1;
			enc_config.width = xres;
			enc_config.height = yres;
			enc_config.num_planes = 1;
			enc_config.scaled_width = xres;
			enc_config.scaled_height = yres;
			enc_config.sizeJpeg = xres * yres * 2;
			enc_config.pix.enc_fmt.in_fmt = V4L2_PIX_FMT_YUYV;
			enc_config.pix.enc_fmt.out_fmt = V4L2_PIX_FMT_JPEG;
			jpeghal_enc_setconfig(vfd, &enc_config);
			jpeghal_enc_getconfig(vfd, &enc_config);
			in_buf.memory = V4L2_MEMORY_MMAP;
			in_buf.num_planes = 1;
			jpeghal_set_inbuf(vfd, &in_buf);
			out_buf.memory = V4L2_MEMORY_MMAP;
			out_buf.num_planes = 1;
			jpeghal_set_outbuf(vfd, &out_buf);
			clock_gettime(CLOCK_MONOTONIC, &tp);
			sprintf(img_name, "img%x%x.jpg", tp.tv_sec, tp.tv_nsec);
			fd_img = open(img_name, O_CREAT | O_RDWR);
			pthread_mutex_lock(&snap_lock);
			memcpy(in_buf.start[0], v4l2_buf[vbuf_param.index].addr[0], 
				v4l2_buf[vbuf_param.index].bytes[0]);
			pthread_mutex_unlock(&snap_lock);
			jpeghal_enc_exe(vfd, &in_buf, &out_buf);
			// v4l2_ioctl(vfd, VIDIOC_LOG_STATUS, NULL);
			write(fd_img, out_buf.start[0], out_buf.bytesused);
			close(fd_img);
			jpeghal_deinit(vfd, &in_buf, &out_buf);
			
		}	
		else
		{
			continue;
		}
	}

}


