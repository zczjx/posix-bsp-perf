/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: bsp_v4l2_cap.c 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2018-8-1    
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
#include <time.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/mman.h>


int bsp_v4l2_open_dev(const char *dev_path)
{
	int vfd, err;
	struct v4l2_capability v4l2_cap;

	if(NULL == dev_path)
	{
		printf("%s NULL ptr dev_path \n", __FUNCTION__);
		return -1;

	}

	vfd = open(dev_path, O_RDWR);
	
	if (vfd < 0)
    {
    	fprintf(stderr, "could not open %s\n", dev_path);
		return -1;
	}
	
	memset(&v4l2_cap, 0, sizeof(struct v4l2_capability));
	err = ioctl(vfd, VIDIOC_QUERYCAP, &v4l2_cap);
	
	if (err < 0)
    {
    	fprintf(stderr, "could not VIDIOC_QUERYCAP\n");
		return -1;
	}

	if(!(v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "/dev/video0 is not V4L2_CAP_VIDEO_CAPTURE \n");
		return -1;
	}

	return vfd;

}

int bsp_v4l2_try_setup(int fd, struct bsp_v4l2_param *val)
{
	struct v4l2_fmtdesc fmt_dsc;
	struct v4l2_format v4l2_fmt;
	struct v4l2_streamparm streamparm;
	int err = 0;

	if(NULL == val)
	{
		printf("%s NULL ptr val \n", __FUNCTION__);
		return -1;

	}
	
	memset(&fmt_dsc, 0, sizeof(struct v4l2_fmtdesc));
	fmt_dsc.index = 0;
	fmt_dsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	err = ioctl(fd, VIDIOC_ENUM_FMT, &fmt_dsc);

	if (err < 0)
	{
		printf("VIDIOC_ENUM_FMT fail \n");
		return -1;
	}

	memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
	v4l2_fmt.type = fmt_dsc.type;
	v4l2_fmt.fmt.pix.pixelformat = val->pixelformat;
	v4l2_fmt.fmt.pix.width		 = val->xres;
	v4l2_fmt.fmt.pix.height 	 = val->yres;
	v4l2_fmt.fmt.pix.field		 = V4L2_FIELD_ANY;
	err = ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt); 

	if (err) 
	{
		printf("VIDIOC_S_FMT fail \n");
		return -1;		
	}

	err = ioctl(fd, VIDIOC_G_FMT, &v4l2_fmt);

	if (err < 0)
	{
		printf("[/dev/video0]:VIDIOC_G_FMT fail \n");
		return -1;
	}

	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	err = ioctl(fd, VIDIOC_G_PARM, &streamparm);
		
	if (err) 
	{
		printf("VIDIOC_G_PARM failed \n");
		return -1;			
	}
	
	val->pixelformat = v4l2_fmt.fmt.pix.pixelformat;
	val->xres = v4l2_fmt.fmt.pix.width;
	val->yres = v4l2_fmt.fmt.pix.height;
	val->fps = (__u32) (streamparm.parm.capture.timeperframe.denominator
				/ streamparm.parm.capture.timeperframe.numerator);

	return 0;
	
}

int bsp_v4l2_req_buf(int fd, struct bsp_v4l2_cap_buf buf_arr[], 
							 int buf_count)
{
	struct v4l2_buffer v4l2_buf_param;
	struct v4l2_requestbuffers req_bufs;
	int err, i = 0;

	if(NULL == buf_arr)
	{
		printf("%s NULL ptr buf_arr \n", __FUNCTION__);
		return -1;

	}

	/* init buf */
	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
    req_bufs.count = buf_count;
    req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_bufs.memory = V4L2_MEMORY_MMAP;
    err = ioctl(fd, VIDIOC_REQBUFS, &req_bufs);
	
    if (err) 
    {
    	printf("req buf error!\n");
        return -1;        
    }
	
	for(i = 0; i < buf_count; i++)
	{
		memset(&v4l2_buf_param, 0, sizeof(struct v4l2_buffer));
		v4l2_buf_param.index = i;
        v4l2_buf_param.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf_param.memory = V4L2_MEMORY_MMAP;
        err = ioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf_param);
			
        if (err) 
		{
			printf("cannot mmap v4l2 buf!\n");
			return -1;
        }
			
		buf_arr[i].bytes = v4l2_buf_param.length;
		buf_arr[i].addr = mmap(0 , buf_arr[i].bytes, 
								PROT_READ, MAP_SHARED, fd, 
								v4l2_buf_param.m.offset);
		err = ioctl(fd, VIDIOC_QBUF, &v4l2_buf_param);
			
		if (err) 
		{
			printf("cannot VIDIOC_QBUF in mmap!\n");
			return -1;
        }

	}

	return 0;

}

int bsp_v4l2_get_frame(int fd, int *buf_idx)
{
	struct pollfd fd_set[1];
	struct v4l2_buffer v4l2_buf_param;
	int err = 0;

	fd_set[0].fd     = fd;
    fd_set[0].events = POLLIN;
    err = poll(fd_set, 1, -1);

	memset(&v4l2_buf_param, 0, sizeof(struct v4l2_buffer));
    v4l2_buf_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_buf_param.memory = V4L2_MEMORY_MMAP;
    err = ioctl(fd, VIDIOC_DQBUF, &v4l2_buf_param);
		
	if (err < 0) 
	{
		printf("cannot VIDIOC_DQBUF \n");
		return -1;
    }
		
	*buf_idx = v4l2_buf_param.index;
	
	return 0;
}

int bsp_v4l2_put_frame_buf(int fd, int buf_idx)
{
	struct v4l2_buffer v4l2_buf_param;
	int err = 0;
	
	memset(&v4l2_buf_param, 0, sizeof(struct v4l2_buffer));
	v4l2_buf_param.index  = buf_idx;
	v4l2_buf_param.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	v4l2_buf_param.memory = V4L2_MEMORY_MMAP;	
	err = ioctl(fd, VIDIOC_QBUF, &v4l2_buf_param);
	
	if (err < 0) 
	{
		printf("cannot VIDIOC_QBUF \n");
		return -1;
	}

	return 0;

}


int bsp_v4l2_stream_on(int fd)
{
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int err = 0;
	
	err = ioctl(fd, VIDIOC_STREAMON, &video_type);
	
	if (err < 0) 
    {
		printf("cannot VIDIOC_STREAMON \n");
		return -1;
    }

	return 0;

}

int bsp_v4l2_stream_off(int fd)
{
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int err = 0;
	
	err = ioctl(fd, VIDIOC_STREAMOFF, &video_type);
	
	if (err < 0) 
    {
		printf("cannot VIDIOC_STREAMOFF \n");
		return -1;
    }

	return 0;

}

int bsp_v4l2_close_dev(int fd)
{

	close(fd);
	return 0;
}

void bsp_print_fps(const char *fsp_dsc, long *fps, 
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

