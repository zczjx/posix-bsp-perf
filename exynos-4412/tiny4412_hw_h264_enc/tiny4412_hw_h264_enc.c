/*******************************************************************************
* Copyright (C), 2000-2019,  Electronic Technology Co., Ltd.
*                
* @filename: tiny4412_hw_h264_enc.c 
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
#include <libv4l2.h>
#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <libv4l-plugin.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <gmodule.h>
#include "bsp_fb.h"
#include "bsp_v4l2_cap.h"

#define LIBV4L2_BUF_NR (4)
#define LIBV4L2_MAX_FMT (16)
#define MAX_PLANES_NR (4)
#define DEFAULT_DISP_XRES (640)
#define DEFAULT_DISP_YRES (480)
#define CODEC_NUM_PLANES (2)
#define V4L2_OUTPUT_BUF_NR (2)
#define V4L2_CAP_BUF_NR (4)
#define H264_ENC_DEF_CTRL_VAL (0)
#define DEFAULT_OUTPUT_BUF_IDX (0)
#define MAX_H264_BUF_SIZE (1*1024*1024)

typedef struct convert_dsc {
	int fd_cov; 
	char *cov_path;
	int bcnt;
	struct bsp_v4l2_buf cov_output_buf[V4L2_OUTPUT_BUF_NR];
	struct bsp_v4l2_buf cov_cap_buf[V4L2_CAP_BUF_NR];
	int cov_mp_flag;
} convert_dsc;

static int disp_fd = 0;
static pthread_mutex_t record_lock = PTHREAD_MUTEX_INITIALIZER;
static int record_run = 0;
static struct bsp_v4l2_buf src_buf[LIBV4L2_BUF_NR];
static struct v4l2_buffer vbuf_param;
static struct bsp_fb_var_attr fb_var_attr;
static struct bsp_fb_fix_attr fb_fix_attr;
static struct rgb_frame disp_frame = {
	.addr = NULL,
};

static int convert_to_spec_pix_fmt(struct convert_dsc *cvt,
	struct bsp_v4l2_buf *dst, __u32 dst_fmt, 
	struct bsp_v4l2_buf *src, __u32 src_fmt);

static void *h264_record_thread(void *arg);

static void libv4l2_print_fps(const char *fsp_dsc, long *fps, 
	long *pre_time, long *curr_time);

int main(int argc, char **argv)
{
	struct v4l2_format fmt;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmtdsc;
	struct v4l2_requestbuffers req_bufs;
	struct bsp_v4l2_param v4l2_param;
	struct pollfd fd_set[1];
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i, buf_cnt, err, pts = 0;
	int fps = 0;
	int buf_mp_flag = 0;
	long pre_time = 0;
	long curr_time = 0;
	char pixformat[32];
	char *ret;
	char *src_path, *enc_path;
	struct stat image_stat;
	int fd_src;
	int rd_size, total_size= 0;
	pthread_t record_tid;
	struct bsp_v4l2_buf rgb_frame;
	struct convert_dsc rgb_conversion;
	char *rgb_conv_path = NULL;
	char *nv12mt_conv_path = NULL;
	
	if(argc < 5)
	{
		printf("usage: tiny4412_hw_h264_enc [source_path] [encode_path]\
		[rgb32_conversion] [nv12mt conversion]\n");
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
	rgb_conv_path = argv[3];
	nv12mt_conv_path = argv[4];
	fd_src = bsp_v4l2_open_dev(src_path, &buf_mp_flag);
	v4l2_param.pixelformat = V4L2_PIX_FMT_YUYV;
	v4l2_param.xres = DEFAULT_DISP_XRES;
	v4l2_param.yres = DEFAULT_DISP_YRES;
	bsp_v4l2_try_setup(fd_src, &v4l2_param, (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	printf("v4l2_param.fps: %d\n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x\n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d\n", v4l2_param.xres);
	printf("v4l2_param.yres: %d\n", v4l2_param.yres);
	buf_cnt = bsp_v4l2_req_buf(fd_src, src_buf, LIBV4L2_BUF_NR, (buf_mp_flag ?
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

	record_run = 1;
	err = pthread_create(&record_tid, NULL, h264_record_thread, argv);
	disp_frame.xres = DEFAULT_DISP_XRES;
	disp_frame.yres = DEFAULT_DISP_YRES;
	disp_frame.bits_per_pixel = fb_var_attr.bits_per_pixel;
	disp_frame.bytes_per_line = disp_frame.xres * (disp_frame.bits_per_pixel >> 3);
	disp_frame.bytes = disp_frame.bytes_per_line * disp_frame.yres;
			
	if(NULL == disp_frame.addr)
	{
		disp_frame.addr = malloc(disp_frame.bytes);
	}

	rgb_conversion.cov_path = rgb_conv_path;
	rgb_conversion.fd_cov = -1;
	
	while(++pts <= 1000)
	{
		struct pollfd fd_set[1];

		fd_set[0].fd = fd_src;
		fd_set[0].events = POLLIN;
		err = poll(fd_set, 1, -1);
		err = bsp_v4l2_get_frame_buf(fd_src, &vbuf_param, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
			(buf_mp_flag ? 1 : 0));
		
		if(buf_mp_flag)
		{
			for(i = 0; i < 1; i++)
			{
				src_buf[vbuf_param.index].bytes[i] = 
					vbuf_param.m.planes[i].bytesused;
			}
		}
		else
		{
			src_buf[vbuf_param.index].bytes[0] = vbuf_param.bytesused;
		}

		src_buf[vbuf_param.index].planes_num = 1;
		src_buf[vbuf_param.index].xres = v4l2_param.xres;
		src_buf[vbuf_param.index].yres = v4l2_param.yres;

		rgb_frame.planes_num = 1;
		rgb_frame.xres = disp_frame.xres;
		rgb_frame.yres = disp_frame.yres;
		rgb_frame.bytes[0] = disp_frame.bytes;
		rgb_frame.addr[0] = disp_frame.addr;
		convert_to_spec_pix_fmt(&rgb_conversion, &rgb_frame, V4L2_PIX_FMT_BGR32,
			&src_buf[vbuf_param.index], V4L2_PIX_FMT_YUYV);
		pthread_mutex_lock(&record_lock);
		err = bsp_v4l2_put_frame_buf(fd_src, &vbuf_param);
		pthread_mutex_unlock(&record_lock);
		bsp_fb_flush(disp_fd, &fb_var_attr, &fb_fix_attr, &disp_frame);
		// libv4l2_print_fps("h264 enc fps: ", &fps, &pre_time, &curr_time);
	}
	
	record_run = 0;
	bsp_v4l2_stream_off(fd_src, 
		(buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	close(fd_src);
	sleep(1);
	pthread_cancel(record_tid);
	printf("h264 enc finish return 0\n");
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

static void *h264_record_thread(void *arg)
{
	int i, buf_cnt = 0;
	int ret = 0;
	struct timespec tp;
	int fd_h264 = -1;
	int fd_enc;
	int buf_mp_flag = 0;
	char *h264_f_addr;
	char cmd;
	char h264_name[64];
	char **argv = arg;
	char *enc_path, *nv12mt_conv_path;
	struct pollfd poll_events[1];
    int poll_state;
	int h264_enc_running = 0;
	struct v4l2_format fmt;
	struct bsp_v4l2_buf nv12mt_frame;
	struct bsp_v4l2_param v4l2_param;
	struct convert_dsc nv12mt_conversion;
	struct v4l2_ext_control ext_ctrl[38];
    struct v4l2_ext_controls ext_ctrls;
	struct v4l2_buffer enc_buf_param;
	struct v4l2_plane mplanes[CODEC_NUM_PLANES];
	struct bsp_v4l2_buf output_buf[V4L2_OUTPUT_BUF_NR];
	struct bsp_v4l2_buf cap_buf[V4L2_CAP_BUF_NR];

	enc_path = argv[2];
	nv12mt_conv_path = argv[4];
	nv12mt_conversion.cov_path = nv12mt_conv_path;
	nv12mt_conversion.fd_cov = -1;

	while(1)
	{
		printf("enter 'r' to do record: \n");
		scanf("%c", &cmd);
		
		if('r' == cmd)
		{
			clock_gettime(CLOCK_MONOTONIC, &tp);
			sprintf(h264_name, "mfc%x%x.h264", tp.tv_sec, tp.tv_nsec);
			fd_h264 = open(h264_name, O_CREAT | O_RDWR);
		
			// init h264 enc
			fd_enc = bsp_v4l2_open_dev(enc_path, &buf_mp_flag);
			ext_ctrl[0].id = V4L2_CID_MPEG_VIDEO_H264_PROFILE;
        	ext_ctrl[0].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[1].id = V4L2_CID_MPEG_VIDEO_H264_LEVEL;
			ext_ctrl[1].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[2].id = V4L2_CID_MPEG_VIDEO_GOP_SIZE;
			ext_ctrl[2].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[3].id = V4L2_CID_MPEG_MFC51_VIDEO_H264_NUM_REF_PIC_FOR_P;
			ext_ctrl[3].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[4].id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE;
			ext_ctrl[4].value = H264_ENC_DEF_CTRL_VAL;  /* 0: one, 1: fixed #mb, 3: fixed #bytes */
            ext_ctrl[5].id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB;
            ext_ctrl[5].value = 1;  /* default */
            ext_ctrl[6].id = V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES;
            ext_ctrl[6].value = 1900; /* default */
			
        /*
        It should be set using h264_arg->NumberBFrames after being handled by appl.
         */
			ext_ctrl[7].id =  V4L2_CID_MPEG_VIDEO_B_FRAMES;
			ext_ctrl[7].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[8].id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE;
			ext_ctrl[8].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[9].id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA;
			ext_ctrl[9].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[10].id = V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA;
			ext_ctrl[10].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[11].id = V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE;
			ext_ctrl[11].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[12].id = V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM;
			ext_ctrl[12].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[13].id = V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB;
			ext_ctrl[13].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[14].id = V4L2_CID_MPEG_MFC51_VIDEO_PADDING;
			ext_ctrl[14].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[15].id = V4L2_CID_MPEG_MFC51_VIDEO_PADDING_YUV;
			ext_ctrl[15].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[16].id = V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE;
			ext_ctrl[16].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[17].id = V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE;
			ext_ctrl[17].value = H264_ENC_DEF_CTRL_VAL;
			ext_ctrl[18].id = V4L2_CID_MPEG_VIDEO_BITRATE;
			ext_ctrl[18].value = 9216000;
			ext_ctrl[19].id = V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP;
			ext_ctrl[19].value = 28;
	        ext_ctrl[20].id =  V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP;
	        ext_ctrl[20].value = 28 + 1;
	        ext_ctrl[21].id =  V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP;
	        ext_ctrl[21].value = 28 + 3;
	        ext_ctrl[22].id =  V4L2_CID_MPEG_VIDEO_H264_MAX_QP;
	        ext_ctrl[22].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[23].id = V4L2_CID_MPEG_VIDEO_H264_MIN_QP;
	        ext_ctrl[23].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[24].id = V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF;
	        ext_ctrl[24].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[25].id =  V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_DARK;
	        ext_ctrl[25].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[26].id =  V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_SMOOTH;
	        ext_ctrl[26].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[27].id =  V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_STATIC;
	        ext_ctrl[27].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[28].id =  V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_ACTIVITY;
	        ext_ctrl[28].value = H264_ENC_DEF_CTRL_VAL;

        /* doesn't have to be set */
	        ext_ctrl[29].id = V4L2_CID_MPEG_VIDEO_GOP_CLOSURE;
	        ext_ctrl[29].value = H264_ENC_DEF_CTRL_VAL;
	        ext_ctrl[30].id = V4L2_CID_MPEG_VIDEO_H264_I_PERIOD;
	        ext_ctrl[30].value = 10;

       /* ENC_FRAME_SKIP_MODE_DISABLE (default) */
            ext_ctrl[31].id = V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE;
            ext_ctrl[31].value = V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_DISABLED;
   
	        ext_ctrl[32].id = V4L2_CID_MPEG_VIDEO_HEADER_MODE;
	        ext_ctrl[32].value = 0; 			/* 0: seperated header
	                                              1: header + first frame */

	        ext_ctrl[33].id = V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT;
	        ext_ctrl[33].value = 1;

	        ext_ctrl[34].id =  V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE;
	        ext_ctrl[34].value = 0;
	        ext_ctrl[35].id = V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC;
	        ext_ctrl[35].value = 0;

	        ext_ctrl[36].id = V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH;
	        ext_ctrl[36].value = 0;
	        ext_ctrl[37].id =  V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT;
	        ext_ctrl[37].value = 0;
			ext_ctrls.count = 38;
        	ext_ctrls.controls = ext_ctrl;

			ret = ioctl(fd_enc, VIDIOC_S_EXT_CTRLS, &ext_ctrls);
			
    		if (ret != 0) 
			{
        		printf("[%s] Failed to set extended controls\n",__func__);
				pthread_exit(ret);
			}

			v4l2_param.xres = DEFAULT_DISP_XRES;
			v4l2_param.yres = DEFAULT_DISP_YRES;
			v4l2_param.pixelformat = V4L2_PIX_FMT_NV12MT;
			v4l2_param.planes_num = 2;
			v4l2_param.req_buf_size[0] = v4l2_param.xres * v4l2_param.yres;
			v4l2_param.req_buf_size[1] = v4l2_param.xres * v4l2_param.yres;
			bsp_v4l2_try_setup(fd_enc, &v4l2_param, (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));
			buf_cnt = bsp_v4l2_req_buf(fd_enc, output_buf, 1, 
						(buf_mp_flag ? 
						V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT), 
						v4l2_param.planes_num);
			output_buf[0].xres = v4l2_param.xres;
			output_buf[0].yres = v4l2_param.yres;
			output_buf[0].planes_num = v4l2_param.planes_num;
				
			printf("buf_cnt: %d, line: %d\n", buf_cnt, __LINE__);
			printf("output_buf[0].bytes[0]: %d, line: %d\n", output_buf[0].bytes[0], __LINE__);
			printf("output_buf[0].bytes[1]: %d, line: %d\n", output_buf[0].bytes[1], __LINE__);
			
			if(buf_cnt < 0)
			{
				printf("bsp_v4l2_req_buf failed err: %d line: %d\n", buf_cnt, __LINE__);
				pthread_exit(-1);
			}

			v4l2_param.xres = DEFAULT_DISP_XRES;
			v4l2_param.yres = DEFAULT_DISP_YRES;
			v4l2_param.pixelformat = V4L2_PIX_FMT_H264;
			v4l2_param.planes_num = 1;
			v4l2_param.req_buf_size[0] = MAX_H264_BUF_SIZE;
			bsp_v4l2_try_setup(fd_enc, &v4l2_param, (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
			buf_cnt = bsp_v4l2_req_buf(fd_enc, cap_buf, V4L2_CAP_BUF_NR, 
						(buf_mp_flag ? 
						V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE), 
						v4l2_param.planes_num);
			
			printf("buf_cnt: %d, line: %d\n", buf_cnt, __LINE__);
			printf("cap_buf[0].bytes[0]: %d, line: %d\n", cap_buf[0].bytes[0], __LINE__);
			
			if(buf_cnt < 0)
			{
				printf("bsp_v4l2_req_buf failed err: %d line: %d\n", buf_cnt, __LINE__);
				pthread_exit(-1);
			}

			for(i = 0; i < buf_cnt; i++)
			{
				int j = 0;
				memset(&enc_buf_param, 0, sizeof(struct v4l2_buffer));
				enc_buf_param.index = i;
				enc_buf_param.type = (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
				enc_buf_param.memory = V4L2_MEMORY_MMAP;
				enc_buf_param.m.planes = mplanes;
				enc_buf_param.length = v4l2_param.planes_num;
				cap_buf[i].xres = v4l2_param.xres;
				cap_buf[i].yres = v4l2_param.yres;
				ret = ioctl(fd_enc, VIDIOC_QUERYBUF, &enc_buf_param);
			
				if (ret) 
				{
					printf("VIDIOC_QUERYBUF err: %d line: %d\n", ret, __LINE__);
					pthread_exit(-1);
    			}
		
				for(j = 0; j < 1; j++)
				{
					enc_buf_param.m.planes[j].bytesused = cap_buf[i].bytes[j];
				}
		
				ret = bsp_v4l2_put_frame_buf(fd_enc, &enc_buf_param);
		
				if(ret < 0)
				{
					printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", ret, __LINE__);
					pthread_exit(-1);
				}
			}

			ret = bsp_v4l2_stream_on(fd_enc, (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));

			if(ret < 0)
			{
				printf("bsp_v4l2_stream_on failed err: %d line: %d\n", ret, __LINE__);
				pthread_exit(-1);
			}

			poll_events[0].fd = fd_enc;
			poll_events[0].events = POLLIN | POLLERR;
			poll_events[0].revents = 0;
			poll_state = poll(poll_events, 1, -1);

			if(poll_state < 0)
			{
				printf("h264 enc system err: %d, line: %d\n", poll_state, __LINE__);
				pthread_exit(-1);
			}

			if (poll_events[0].revents & POLLIN)
			{
				memset(&enc_buf_param, 0, sizeof(struct v4l2_buffer));
				enc_buf_param.type = (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
				enc_buf_param.memory = V4L2_MEMORY_MMAP;
				enc_buf_param.m.planes = mplanes;
				enc_buf_param.length = 1;
				ret = bsp_v4l2_get_frame_buf(fd_enc, &enc_buf_param, (buf_mp_flag ? 
						V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE), 1);
			
				if(ret < 0)
				{
					printf("bsp_v4l2_get_frame_buf err: %d line :%d\n", ret, __LINE__);
					pthread_exit(-1);
				}
			
			}
			else if(poll_events[0].revents & POLLERR)
			{
				printf("h264 enc system POLLERR line: %d\n",  __LINE__);
				pthread_exit(-1);
			}

			cap_buf[enc_buf_param.index].bytes[0] = 
				enc_buf_param.m.planes[0].bytesused;
			printf("write file bytes: %d, line: %d \n", 
				cap_buf[enc_buf_param.index].bytes[0], __LINE__);
			write(fd_h264, cap_buf[enc_buf_param.index].addr[0], 
				cap_buf[enc_buf_param.index].bytes[0]);
			ret = bsp_v4l2_put_frame_buf(fd_enc, &enc_buf_param);
			break;
			
		}	

	}
	
	while(1 == record_run)
	{
		// Setup of the dec requests
		memset(&enc_buf_param, 0, sizeof(struct v4l2_buffer));
		enc_buf_param.index = DEFAULT_OUTPUT_BUF_IDX;
		enc_buf_param.type = (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
		enc_buf_param.memory = V4L2_MEMORY_MMAP;
		enc_buf_param.m.planes = mplanes;
		enc_buf_param.length = 2;
			
		ret = ioctl(fd_enc, VIDIOC_QUERYBUF, &enc_buf_param);
						
		if (ret) 
		{
			printf("VIDIOC_QUERYBUF err: %d line: %d\n", ret, __LINE__);
			pthread_exit(-1);
		}

		
		nv12mt_frame.planes_num = output_buf[0].planes_num;
		nv12mt_frame.xres = output_buf[0].xres;
		nv12mt_frame.yres = output_buf[0].yres;
		
		for(i = 0; i < nv12mt_frame.planes_num ; i++)
		{	
			nv12mt_frame.bytes[i] = output_buf[0].bytes[i];
			nv12mt_frame.addr[i] = output_buf[0].addr[i];
		}

		pthread_mutex_lock(&record_lock);
		ret = convert_to_spec_pix_fmt(&nv12mt_conversion, 
								&nv12mt_frame, V4L2_PIX_FMT_NV12MT,
								&src_buf[vbuf_param.index], V4L2_PIX_FMT_YUYV);
		pthread_mutex_unlock(&record_lock);
		
		if (ret < 0) 
		{
			printf("convert_to_spec_pix_fmt err: %d line: %d\n", ret, __LINE__);
			pthread_exit(-1);
		}

		for(i = 0; i < nv12mt_frame.planes_num ; i++)
		{	
			enc_buf_param.m.planes[i].bytesused = output_buf[0].bytes[i];
		}

		ret = bsp_v4l2_put_frame_buf(fd_enc, &enc_buf_param);
			
		if(ret < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", ret, __LINE__);
			pthread_exit(-1);
		}

		if(0 == h264_enc_running)
		{
			ret = bsp_v4l2_stream_on(fd_enc, (buf_mp_flag ? 
						V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));
			
			if(ret < 0)
			{
				printf("bsp_v4l2_stream_on failed err: %d line: %d\n", ret, __LINE__);
				pthread_exit(-1);
			}
		
		}
			
		poll_events[0].fd = fd_enc;
		poll_events[0].events = POLLIN | POLLERR;
		poll_events[0].revents = 0;
		poll_state = poll(poll_events, 1, -1);

		if(poll_state < 0)
		{
			printf("h264 enc system err: %d, line: %d\n", poll_state, __LINE__);
			pthread_exit(-1);
		}

		if (poll_events[0].revents & POLLIN)
		{
			memset(&enc_buf_param, 0, sizeof(struct v4l2_buffer));
			enc_buf_param.type = (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
			enc_buf_param.memory = V4L2_MEMORY_MMAP;
			enc_buf_param.m.planes = mplanes;
			enc_buf_param.length = 1;
			ret = bsp_v4l2_get_frame_buf(fd_enc, &enc_buf_param, (buf_mp_flag ? 
						V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE), 1);
			
			if(ret < 0)
			{
				printf("bsp_v4l2_get_frame_buf err: %d line :%d\n", ret, __LINE__);
				pthread_exit(-1);
			}
			
		}
		else if(poll_events[0].revents & POLLERR)
		{
			printf("h264 enc system POLLERR line: %d\n",  __LINE__);
			pthread_exit(-1);
		}

		cap_buf[enc_buf_param.index].bytes[0] = 
				enc_buf_param.m.planes[0].bytesused;
		write(fd_h264, cap_buf[enc_buf_param.index].addr[0], 
				cap_buf[enc_buf_param.index].bytes[0]);
		ret = bsp_v4l2_put_frame_buf(fd_enc, &enc_buf_param);
		memset(&enc_buf_param, 0, sizeof(struct v4l2_buffer));
		enc_buf_param.type = (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
		enc_buf_param.memory = V4L2_MEMORY_MMAP;
		enc_buf_param.m.planes = mplanes;
		enc_buf_param.length = 2;
		ret = bsp_v4l2_get_frame_buf(fd_enc, &enc_buf_param, (buf_mp_flag ? 
						V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT), 
						enc_buf_param.length);
				
		if(ret < 0)
		{
			printf("bsp_v4l2_get_frame err: %d, line: %d\n", ret, __LINE__);
			pthread_exit(-1);
		}
	
		h264_enc_running = 1;
		
	}

	bsp_v4l2_stream_off(fd_enc, (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));
	bsp_v4l2_stream_off(fd_enc, (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	close(fd_enc);
	close(fd_h264);

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

