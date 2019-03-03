/*******************************************************************************
* Copyright (C), 2000-2019,  Electronic Technology Co., Ltd.
*                
* @filename: tiny4412_hw_h264_dec.c 
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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <gmodule.h>
#include "bsp_fb.h"
#include "bsp_v4l2_cap.h"

#define V4L2_OUTPUT_BUF_NR (2)
#define V4L2_CAP_BUF_NR (4)
#define MFC_MAX_CAP_BUF_NR (18)
#define LIBV4L2_MAX_FMT (16)
#define MAX_DECODER_INPUT_BUFFER_SIZE  (1024 * 2048)
#define CODEC_NUM_PLANES (2)
#define DEFAULT_OUTPUT_BUF_IDX (0)

#define H264_NAL_TYPE_IDR 0x05
#define H264_NAL_TYPE_SEI 0x06
#define H264_NAL_TYPE_SPS 0x07
#define H264_NAL_TYPE_PPS 0x08

#define H264_NAL_TYPE_RESVERD 0x0F


#define DEFAULT_DISP_XRES (800)
#define DEFAULT_DISP_YRES (480)

typedef struct convert_dsc {
	int fd_cov; 
	char *cov_path;
	int bcnt;
	struct bsp_v4l2_buf cov_output_buf[V4L2_OUTPUT_BUF_NR];
	struct bsp_v4l2_buf cov_cap_buf[V4L2_CAP_BUF_NR];
	int cov_mp_flag;
} convert_dsc;

static int disp_fd = 0;
static struct bsp_fb_var_attr fb_var_attr;
static struct bsp_fb_fix_attr fb_fix_attr;
static struct rgb_frame disp_frame = {
	.addr = NULL,
};

static int h264_start_pos = 0;
static int h264_current_pos = 0;
static int h264_end_pos = 0;

static int convert_to_spec_pix_fmt(struct convert_dsc *cvt,
	struct bsp_v4l2_buf *dst, __u32 dst_fmt, 
	struct bsp_v4l2_buf *src, __u32 src_fmt);

static void libv4l2_print_fps(const char *fsp_dsc, long *fps, 
	long *pre_time, long *curr_time);


static int parser_next_h264_header(unsigned char *start_addr, int src_len, 
	unsigned char **head_buf, int *head_buf_len);

static int get_next_h264_nalu(unsigned char *start_addr, int src_len, 
	int *nalu_start_pos, int *nalu_end_pos);

int main(int argc, char **argv)
{
	struct v4l2_format fmt;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmtdsc;
	struct v4l2_requestbuffers req_bufs;
	struct v4l2_pix_format_mplane *pix_mp;
	struct v4l2_buffer vbuf_param;
	struct bsp_v4l2_param v4l2_param;
	struct v4l2_crop crop;
	struct v4l2_plane mplanes[CODEC_NUM_PLANES];
	struct pollfd fd_set[1];
	int poll_state;
	int buf_mp_flag = 0;
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int i, err, buf_cnt = 0;
	int fps = 0;
	long pre_time = 0;
	long curr_time = 0;
	char pixformat[32];
	char *ret;
	char *src_path, *dec_path, *cov_path;
	struct stat image_stat;
	int fd_dec;
	int rd_size, total_size= 0;
	pthread_t snap_tid;
	int fd_img = -1;
	unsigned char *src_file_addr = NULL;
	struct stat f_img_stat;
	unsigned char *head_buf;
	int head_buf_len;
	int f_curr_pos = 0;
	int delta_pos = 0;
	int nalu_start_from_curr_pos;
	int nalu_end_from_curr_pos;
	struct bsp_v4l2_buf rgb_frame;
	struct convert_dsc rgb_conversion;
	struct bsp_v4l2_buf output_buf[V4L2_OUTPUT_BUF_NR];
	struct bsp_v4l2_buf cap_buf[MFC_MAX_CAP_BUF_NR];
	
	if(argc < 4)
	{
		printf("usage: tiny4412_hw_h264_dec [decoder path] [coversion path] [source file]\n");
		return -1;
	}

	dec_path = argv[1];
	src_path = argv[3];
	rgb_conversion.cov_path = argv[2];
	rgb_conversion.fd_cov = -1;
	fd_img = open(src_path, O_RDONLY);

	if(fd_img < 0)
	{
		printf("image open failed fd: %d line: %d\n", fd_img, __LINE__);
		return -1;
	}

	if(fstat(fd_img, &f_img_stat) < 0)
	{
		printf("fstat failed line: %d\n", __LINE__);
		return -1;
	}

	src_file_addr = mmap(NULL, f_img_stat.st_size, 
						PROT_READ, MAP_SHARED, fd_img, 0);
	
	if(src_file_addr == MAP_FAILED)
	{
		printf("can't mmap this file line: %d\n", __LINE__);
		return -1;
	}

	f_curr_pos = 0;
	delta_pos = parser_next_h264_header(src_file_addr, f_img_stat.st_size, 
					&head_buf, &head_buf_len);
	f_curr_pos += delta_pos;

	if(f_curr_pos < 0)
	{
		printf("can't parser_next_h264_header line: %d\n", __LINE__);
		return -1;
	}

	printf("h264 f_curr_pos: %d\n", f_curr_pos);
	for(i = 0; i < head_buf_len; i++)
	{
		if(0 == (i % 10))
		{
			printf("\n");
		}
		printf("0x%x ", head_buf[i]);

	}

	printf("\n");
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
	
	fd_dec = bsp_v4l2_open_dev(dec_path, &buf_mp_flag);
	v4l2_param.pixelformat = V4L2_PIX_FMT_H264;
	v4l2_param.req_buf_size[0] = MAX_DECODER_INPUT_BUFFER_SIZE;
	v4l2_param.planes_num = 1;
	bsp_v4l2_try_setup(fd_dec, &v4l2_param, (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));
	buf_cnt = bsp_v4l2_req_buf(fd_dec, output_buf, V4L2_OUTPUT_BUF_NR, 
			(buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT), 
			1);

	if(buf_cnt < 0)
	{
		printf("bsp_v4l2_req_buf failed err: %d line: %d\n", buf_cnt, __LINE__);
		return -1;
	}

	memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
	vbuf_param.index = DEFAULT_OUTPUT_BUF_IDX;
    vbuf_param.type = (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
    vbuf_param.memory = V4L2_MEMORY_MMAP;
	vbuf_param.m.planes = mplanes;
	vbuf_param.length = 1;
	err = ioctl(fd_dec, VIDIOC_QUERYBUF, &vbuf_param);
			
	if (err) 
	{
		printf("VIDIOC_QUERYBUF err: %d line: %d\n", err, __LINE__);
		return -1;
    }

	output_buf[DEFAULT_OUTPUT_BUF_IDX].bytes[0] = head_buf_len;
	memcpy(output_buf[0].addr[0], head_buf, head_buf_len);
	vbuf_param.m.planes[0].bytesused = output_buf[0].bytes[0];
	err = bsp_v4l2_put_frame_buf(fd_dec, &vbuf_param);
	if(err < 0)
	{
		printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", err, __LINE__);
		return -1;
	}

	err = bsp_v4l2_stream_on(fd_dec, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));

	if(err < 0)
	{
		printf("bsp_v4l2_stream_on failed err: %d line: %d\n", err, __LINE__);
		return -1;
	}

	memset(&fmt, 0, sizeof(struct v4l2_format));
	fmt.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	err = ioctl(fd_dec, VIDIOC_G_FMT, &fmt);

	if (err < 0)
	{
		printf("VIDIOC_G_FMT fail err: %d line: %d\n", err, __LINE__);
	}

	pix_mp = &fmt.fmt.pix_mp;
	printf("capture pix_mp->width: %d\n", pix_mp->width);
	printf("capture pix_mp->height: %d\n", pix_mp->height);
	printf("capture pix_mp->pixelformat: 0x%x\n", pix_mp->pixelformat);
	printf("capture pix_mp->field: 0x%x\n", pix_mp->field);
	printf("capture pix_mp->colorspace: 0x%x\n", pix_mp->colorspace);
	printf("capture pix_mp->plane_fmt[0].sizeimage: %d\n", 
		pix_mp->plane_fmt[0].sizeimage);
	printf("capture pix_mp->plane_fmt[0].bytesperline: %d\n", 
		pix_mp->plane_fmt[0].bytesperline);
	printf("capture pix_mp->plane_fmt[1].sizeimage: %d\n", 
		pix_mp->plane_fmt[1].sizeimage);
	printf("capture pix_mp->plane_fmt[1].bytesperline: %d\n", 
		pix_mp->plane_fmt[1].bytesperline);
	printf("capture pix_mp->num_planes: %d\n", pix_mp->num_planes);
	printf("capture pix_mp->flags: 0x%x\n", pix_mp->flags);
	printf("capture pix_mp->ycbcr_enc: %d\n", pix_mp->ycbcr_enc);
	printf("capture pix_mp->quantization: %d\n", pix_mp->quantization);

	memset(&crop, 0, sizeof(crop));
    crop.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);

    err = ioctl(fd_dec, VIDIOC_G_CROP, &crop);
	
    if (err != 0) 
	{
        printf("[%s] line: [%d] VIDIOC_G_CROP failed err: %d \n", __func__, __LINE__, err);
    }

	buf_cnt = bsp_v4l2_req_buf(fd_dec, cap_buf, MFC_MAX_CAP_BUF_NR, 
			(buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE), 
			2);

	printf("buf_cnt: %d, line: %d\n", buf_cnt, __LINE__);
	printf("cap_buf[0].bytes[0]: %d, line: %d\n", cap_buf[0].bytes[0], __LINE__);
	printf("cap_buf[0].bytes[1]: %d, line: %d\n", cap_buf[0].bytes[1], __LINE__);

	if(buf_cnt < 0)
	{
		printf("bsp_v4l2_req_buf failed err: %d line: %d\n", buf_cnt, __LINE__);
		return -1;
	}

	for(i = 0; i < buf_cnt; i++)
	{
		int j = 0;
		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.index = i;
		vbuf_param.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = mplanes;
		vbuf_param.length = pix_mp->num_planes;
		cap_buf[i].planes_num = pix_mp->num_planes;
		cap_buf[i].xres = pix_mp->width;
		cap_buf[i].yres = pix_mp->height;
		err = ioctl(fd_dec, VIDIOC_QUERYBUF, &vbuf_param);
			
		if (err) 
		{
			printf("VIDIOC_QUERYBUF err: %d line: %d\n", err, __LINE__);
			return -1;
    	}
		
		for(j = 0; j < pix_mp->num_planes; j++)
		{
			vbuf_param.m.planes[j].bytesused = cap_buf[0].bytes[j];
		}
		
		err = bsp_v4l2_put_frame_buf(fd_dec, &vbuf_param);
		
		if(err < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", err, __LINE__);
			return -1;
		}
	}

	err = bsp_v4l2_stream_on(fd_dec, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));

	if(err < 0)
	{
		printf("bsp_v4l2_stream_on failed err: %d line: %d\n", err, __LINE__);
		return -1;
	}

	memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
	vbuf_param.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
	vbuf_param.memory = V4L2_MEMORY_MMAP;
	vbuf_param.m.planes = mplanes;
	vbuf_param.length = 1;
	err = bsp_v4l2_get_frame_buf(fd_dec, &vbuf_param, (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT), 1);
	
	if(err < 0)
	{
		printf("bsp_v4l2_get_frame err: %d, line: %d\n", err, __LINE__);
		return -1;
	}

	disp_frame.xres = DEFAULT_DISP_XRES;
	disp_frame.yres = DEFAULT_DISP_YRES;
	disp_frame.bits_per_pixel = fb_var_attr.bits_per_pixel;
	disp_frame.bytes_per_line = disp_frame.xres * (disp_frame.bits_per_pixel >> 3);
	disp_frame.bytes = disp_frame.bytes_per_line * disp_frame.yres;
	printf("disp_frame.bytes: %d, line: %d\n", disp_frame.bytes, __LINE__);
		
	if(NULL == disp_frame.addr)
	{
		disp_frame.addr = malloc(disp_frame.bytes);
	}
		
	delta_pos = 0;
	while(f_curr_pos < (f_img_stat.st_size - 3))
	{
		fd_set[0].fd     = fd_dec;
    	fd_set[0].events = POLLOUT | POLLERR;;
		delta_pos = get_next_h264_nalu((src_file_addr + f_curr_pos), 
						(f_img_stat.st_size - f_curr_pos),
						&nalu_start_from_curr_pos, &nalu_end_from_curr_pos);
		
		if(delta_pos < 0)
		{
			printf("get_next_h264_nalu err: %d, line: %d\n", delta_pos, __LINE__);
			return -1;
		}
		output_buf[DEFAULT_OUTPUT_BUF_IDX].bytes[0] = delta_pos;
		memcpy(output_buf[0].addr[0], (src_file_addr + f_curr_pos), delta_pos);
		vbuf_param.index = DEFAULT_OUTPUT_BUF_IDX;
		vbuf_param.type = (buf_mp_flag ? 
				V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes[0].bytesused = output_buf[0].bytes[0];
		err = bsp_v4l2_put_frame_buf(fd_dec, &vbuf_param);
		if(err < 0)
		{
			printf("bsp_v4l2_put_frame_buf err: %d, line: %d\n", err, __LINE__);
			return -1;
		}

		poll_state = poll(fd_set, 1, -1);

		if(poll_state < 0)
		{
			printf("h264 dec system err: %d, line: %d\n", poll_state, __LINE__);
			return -1;
		}

		if (fd_set[0].events & POLLOUT)
		{
			vbuf_param.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT);
			vbuf_param.memory = V4L2_MEMORY_MMAP;
			vbuf_param.m.planes = mplanes;
			vbuf_param.length = 1;
			err = bsp_v4l2_get_frame_buf(fd_dec, &vbuf_param, (buf_mp_flag ? 
					V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT),
					1);
			
			if(err < 0)
			{
				printf("bsp_v4l2_get_frame_buf err: %d line :%d\n", err, __LINE__);
			}
			
		}
		else if(fd_set[0].events & POLLERR)
		{
			printf("h264 dec system POLLERR line: %d\n",  __LINE__);
			return -1;
		}

		memset(&vbuf_param, 0, sizeof(struct v4l2_buffer));
		vbuf_param.type = (buf_mp_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
		vbuf_param.memory = V4L2_MEMORY_MMAP;
		vbuf_param.m.planes = mplanes;
		vbuf_param.length = pix_mp->num_planes;
		err = bsp_v4l2_get_frame_buf(fd_dec, &vbuf_param, (buf_mp_flag ? 
				V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE),
				pix_mp->num_planes);
		
		if(err < 0)
		{
			printf("bsp_v4l2_get_frame_buf err: %d, line: %d\n", err, __LINE__);
			f_curr_pos += delta_pos;
			continue;
		}
		else
		{
			rgb_frame.planes_num = 1;
			rgb_frame.xres = disp_frame.xres;
			rgb_frame.yres = disp_frame.yres;
			rgb_frame.bytes[0] = disp_frame.bytes;
			rgb_frame.addr[0] = disp_frame.addr;
			err = convert_to_spec_pix_fmt(&rgb_conversion, &rgb_frame, V4L2_PIX_FMT_BGR32,
						&cap_buf[vbuf_param.index], V4L2_PIX_FMT_NV12MT);

			if(err < 0)
			{
				return -1;
			}

			bsp_fb_flush(disp_fd, &fb_var_attr, &fb_fix_attr, &disp_frame);
		}

		f_curr_pos += delta_pos;
		err = bsp_v4l2_put_frame_buf(fd_dec, &vbuf_param);
		libv4l2_print_fps("h264 dec fps: ", &fps, &pre_time, &curr_time);
	}
	
	bsp_v4l2_stream_off(fd_dec, (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : V4L2_BUF_TYPE_VIDEO_OUTPUT));
	bsp_v4l2_stream_off(fd_dec, (buf_mp_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE));
	close(fd_dec);
	close(fd_img);
	printf("h264 dec finish return 0\n");
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

static int parser_next_h264_header(unsigned char *start_addr, int src_len, 
	unsigned char **head_buf, int *head_buf_len)
{
    int i;
	int found = 0;
	int ret, tmp_pos;
    unsigned char nal_unit_type;
	int tmp_nalu_start_pos;
	int tmp_nalu_end_pos;
	unsigned char *tmp_head_buf_addr[3] = {NULL, NULL, NULL};
	unsigned int tmp_head_buf_len[3] = {0, 0, 0};
	int head_buf_cnt = 0;
	int offset_len = 0;
	int remain_len = src_len;
	char *tmp_start_addr = start_addr;
	unsigned char *tmp_head_buf = NULL;

	if((NULL == start_addr) || (NULL == head_buf_len) 
	|| (src_len <= 0))
	{
		return -1;
	}

	found = 0;
	while((remain_len >= 0) && (head_buf_cnt < 3))
	{
		remain_len -= offset_len;
		tmp_start_addr += offset_len;
		offset_len = get_next_h264_nalu(tmp_start_addr, remain_len,
						&tmp_nalu_start_pos, &tmp_nalu_end_pos);
		
		if(offset_len <= 0)
		{
			*head_buf = NULL;
			*head_buf_len = 0;
			ret = -1;
			goto err;
		}

		
    	if(tmp_start_addr[tmp_nalu_start_pos] == 0 
		&& tmp_start_addr[tmp_nalu_start_pos+1] == 0 
		&& tmp_start_addr[tmp_nalu_start_pos+2] == 0 
		&& tmp_start_addr[i+3] == 0x01) 
    	{
    		// ( next_bits( 32 ) == 0x00000001 )
    		nal_unit_type = tmp_start_addr[tmp_nalu_start_pos + 4] & 0x1f;
    	}
		
		else if(tmp_start_addr[tmp_nalu_start_pos] == 0 
		&& tmp_start_addr[tmp_nalu_start_pos+1] == 0 
		&& tmp_start_addr[tmp_nalu_start_pos+2] == 0x01) 
    	{
    		// ( next_bits( 24 ) == 0x000001 )
    		nal_unit_type = tmp_start_addr[tmp_nalu_start_pos + 3] & 0x1f;
    	}

		switch(nal_unit_type)
		{
			case H264_NAL_TYPE_RESVERD:
				break;
			case H264_NAL_TYPE_SEI:
			case H264_NAL_TYPE_SPS:
			case H264_NAL_TYPE_PPS:
				tmp_head_buf_len[head_buf_cnt] = tmp_nalu_end_pos - tmp_nalu_start_pos + 1;
				tmp_head_buf_addr[head_buf_cnt] = malloc(tmp_head_buf_len[head_buf_cnt]);
				memcpy(tmp_head_buf_addr[head_buf_cnt], 
					(unsigned char *)(tmp_start_addr + tmp_nalu_start_pos), 
					tmp_head_buf_len[head_buf_cnt]);
				head_buf_cnt++;
				found = 1;
				break;
			
			default:
				if(1 == found)
				{
					goto normal_finish;
				}
				break;
		}

	}

	if(0 == found)
	{
		*head_buf = NULL;
		*head_buf_len = 0;
		ret = -1;
		goto err;
	}
	
normal_finish:
	tmp_head_buf = malloc(tmp_head_buf_len[0] + 
				tmp_head_buf_len[1] + tmp_head_buf_len[2]);
	tmp_pos = 0;
	for(i = 0; i < head_buf_cnt; i++)
	{
		memcpy(tmp_head_buf + tmp_pos, tmp_head_buf_addr[i], tmp_head_buf_len[i]);
		tmp_pos += tmp_head_buf_len[i];
	}
	*head_buf = tmp_head_buf;
	*head_buf_len = tmp_pos;
	ret = src_len - remain_len;
	return ret;

err:
	return ret; 

}

static int get_next_h264_nalu(unsigned char *start_addr, int src_len, 
	int *nalu_start_pos, int *nalu_end_pos)
{
	int i;

	if((NULL == start_addr) || (NULL == nalu_start_pos) 
	|| (NULL == nalu_end_pos) || (src_len <= 0))
	{
		return -1;
	}

    // find start
    *nalu_start_pos = 0;
    *nalu_end_pos = 0;
    
    i = 0;
	//( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
    while((start_addr[i] != 0 || start_addr[i+1] != 0 
			|| start_addr[i+2] != 0x01) 
		&& (start_addr[i] != 0 || start_addr[i+1] != 0 
			|| start_addr[i+2] != 0 || start_addr[i+3] != 0x01))
    {
        i++; // skip leading zero
        if (i + 4 >= src_len) 
		{
			return -1;
			// did not find nal start
		} 
    }

	*nalu_start_pos = i;
			
	if(start_addr[i] == 0 && start_addr[i+1] == 0 
	&& start_addr[i+2] == 0 && start_addr[i+3] == 0x01) 
    {
    	// ( next_bits( 32 ) == 0x00000001 )
    	i += 4;
    }
		
	else if(start_addr[i] == 0 && start_addr[i+1] == 0 
	&& start_addr[i+2] == 0x01) 
    {
    	// ( next_bits( 24 ) == 0x000001 )
    	i += 3;
    }

   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
    while((start_addr[i] != 0 || start_addr[i+1] != 0 
			|| start_addr[i+2] != 0x01) 
		&& (start_addr[i] != 0 || start_addr[i+1] != 0 
			|| start_addr[i+2] != 0 || start_addr[i+3] != 0x01))
    {
        i++; // skip leading zero
        if (i + 4 >= src_len)
		{
			*nalu_end_pos = src_len - 1;
			return src_len;
		} 
    }
    
    *nalu_end_pos = i - 1;
    return i;
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

