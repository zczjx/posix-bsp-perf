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
#include <unistd.h>

int bsp_v4l2_open_dev(const char *dev_path, int *mp_buf_flag)
{
	int vfd, err;
	struct v4l2_capability v4l2_cap;

	if((NULL == dev_path) || (NULL == mp_buf_flag)) 
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

	*mp_buf_flag = 0;
	memset(&v4l2_cap, 0, sizeof(struct v4l2_capability));
	err = ioctl(vfd, VIDIOC_QUERYCAP, &v4l2_cap);
	
	if (err < 0)
    {
    	fprintf(stderr, "could not VIDIOC_QUERYCAP\n");
		return -1;
	}

	printf("[%s]: v4l2_cap.capabilities: 0x%x\n", dev_path, v4l2_cap.capabilities);
	if(!((v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)
	|| (v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE_MPLANE)))
	{
		fprintf(stderr, "[%s]is not V4L2_CAP_VIDEO_CAPTURE\n", dev_path);
		return -1;
	}

	if(v4l2_cap.capabilities & 
	(V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE
	| V4L2_CAP_VIDEO_M2M_MPLANE))
	{
		*mp_buf_flag = 1;
	}

	return vfd;

}

int bsp_v4l2_subdev_open(const char *subdev_path)
{

	int fd, err;

	if((NULL == subdev_path)) 
	{
		printf("%s NULL ptr subdev_path \n", __FUNCTION__);
		return -1;

	}

	fd = open(subdev_path, O_RDWR);
	
	if (fd < 0)
    {
    	fprintf(stderr, "could not open %s\n", subdev_path);
		return -1;
	}

	return fd;

}


int bsp_v4l2_try_setup(int fd, struct bsp_v4l2_param *val, 
	int mp_buf_flag)
{
	struct v4l2_format v4l2_fmt;
	struct v4l2_streamparm streamparm;
	int err = 0;

	if(NULL == val)
	{
		printf("%s NULL ptr val \n", __FUNCTION__);
		return -1;

	}
	
	memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
	v4l2_fmt.type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	
	if(1 == mp_buf_flag)
	{
		v4l2_fmt.fmt.pix_mp.pixelformat = val->pixelformat;
    	v4l2_fmt.fmt.pix_mp.width       = val->xres;
		v4l2_fmt.fmt.pix_mp.height      = val->yres;
    	v4l2_fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
	}
	else
	{
		v4l2_fmt.fmt.pix.pixelformat = val->pixelformat;
    	v4l2_fmt.fmt.pix.width       = val->xres;
		v4l2_fmt.fmt.pix.height      = val->yres;
    	v4l2_fmt.fmt.pix.field       = V4L2_FIELD_ANY;
	}

	err = ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt); 

	if (err) 
	{
		printf("VIDIOC_TRY_FMT fail \n");
	
	}

	
	err = ioctl(fd, VIDIOC_G_FMT, &v4l2_fmt);

	if (err < 0)
	{
		printf("VIDIOC_G_FMT fail err: %d\n", err);
	}

	val->pixelformat = v4l2_fmt.fmt.pix.pixelformat;
	val->xres = v4l2_fmt.fmt.pix.width;
	val->yres = v4l2_fmt.fmt.pix.height;

/*********
	memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
	v4l2_fmt.type = V4L2_BUF_TYPE_PRIVATE;
	
	if(1 == mp_buf_flag)
	{
		v4l2_fmt.fmt.pix_mp.pixelformat = val->pixelformat;
    	v4l2_fmt.fmt.pix_mp.width       = val->xres;
		v4l2_fmt.fmt.pix_mp.height      = val->yres;
    	v4l2_fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;
	}
	else
	{
		v4l2_fmt.fmt.pix.pixelformat = val->pixelformat;
    	v4l2_fmt.fmt.pix.width       = val->xres;
		v4l2_fmt.fmt.pix.height      = val->yres;
    	v4l2_fmt.fmt.pix.field       = V4L2_FIELD_ANY;
	}

	err = ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt); 

	if (err) 
	{
		printf("VIDIOC_TRY_FMT fail func: %s, line: %d\n", __FUNCTION__, __LINE__);	
	}
***/

	streamparm.type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	err = ioctl(fd, VIDIOC_G_PARM, &streamparm);
		
	if (0 == err) 
	{
		val->fps = (__u32) (streamparm.parm.capture.timeperframe.denominator
				/ streamparm.parm.capture.timeperframe.numerator);
	}
	else
	{
		fprintf(stderr, "VIDIOC_G_PARM failed \n");
		val->fps = 15;
	}
	
	return 0;
	
}

int bsp_v4l2_req_buf(int fd, struct bsp_v4l2_cap_buf buf_arr[], 
	int buf_count, int mp_buf_flag)
{
	struct v4l2_buffer v4l2_buf_param;
	struct v4l2_requestbuffers req_bufs;
	struct v4l2_create_buffers create_buffers;
	struct v4l2_plane mplane;
	int err, i = 0;

	if(NULL == buf_arr)
	{
		printf("%s NULL ptr buf_arr \n", __FUNCTION__);
		return -1;

	}

	memset(&create_buffers, 0, sizeof(struct v4l2_create_buffers));
	create_buffers.count = buf_count;
	create_buffers.memory = V4L2_MEMORY_MMAP;
	create_buffers.format.type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	err = ioctl(fd, VIDIOC_G_FMT, &create_buffers.format);

	if (err < 0)
	{
		printf("VIDIOC_G_FMT fail err: %d\n", err);
	}

    err = ioctl(fd, VIDIOC_CREATE_BUFS, &create_buffers);
	
    if (err) 
    {
    	printf("VIDIOC_CREATE_BUFS failed err: %d\n", err);     
    }
	
	/* init buf */
	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
    req_bufs.count = buf_count;
    req_bufs.type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
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
        v4l2_buf_param.type = (mp_buf_flag ? 
			V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
        v4l2_buf_param.memory = V4L2_MEMORY_MMAP;
		if(1 == mp_buf_flag)
		{
			v4l2_buf_param.m.planes = &mplane;
			v4l2_buf_param.length = 1;
		}
        err = ioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf_param);
			
        if (err) 
		{
			printf("cannot mmap v4l2 buf err: %d\n", err);
			return -1;
        }

		if(1 == mp_buf_flag)
		{
			buf_arr[i].bytes = v4l2_buf_param.m.planes->length;
			buf_arr[i].addr = mmap(0 , buf_arr[i].bytes, 
								PROT_READ, MAP_SHARED, fd, 
								v4l2_buf_param.m.planes->m.mem_offset);
		}
		else
		{
			buf_arr[i].bytes = v4l2_buf_param.length;
			buf_arr[i].addr = mmap(0 , buf_arr[i].bytes, 
								PROT_READ, MAP_SHARED, fd, 
								v4l2_buf_param.m.offset);
		}
		
		err = ioctl(fd, VIDIOC_QBUF, &v4l2_buf_param);
			
		if (err) 
		{
			printf("cannot VIDIOC_QBUF in mmap!\n");
			return -1;
        }

	}

	printf("bsp_v4l2_req_buf OK\n");
	return 0;

}

int bsp_v4l2_get_frame(int fd, struct v4l2_buffer *buf_param, 
	int mp_buf_flag)
{
	struct pollfd fd_set[1];
	struct v4l2_plane mplane;
	int err = 0;

	if(NULL == buf_param)
	{
		printf("buf_param is NULL %s:%d \n", __FUNCTION__, __LINE__);
		return -1;
	
	}

	fd_set[0].fd     = fd;
    fd_set[0].events = POLLIN;
    err = poll(fd_set, 1, -1);

	memset(buf_param, 0, sizeof(struct v4l2_buffer));
    buf_param->type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
    buf_param->memory = V4L2_MEMORY_MMAP;

	if(1 == mp_buf_flag)
	{
		buf_param->m.planes = &mplane;
		buf_param->length = 1;
	}
		
    err = ioctl(fd, VIDIOC_DQBUF, buf_param);
		
	if (err < 0) 
	{
		printf("cannot VIDIOC_DQBUF func: %s, line: %d\n", __FUNCTION__, __LINE__);
		return -1;
    }
	
	return 0;
}

int bsp_v4l2_put_frame_buf(int fd, struct v4l2_buffer *buf_param)
{
	int err = 0;

	if(NULL == buf_param)
	{
		printf("buf_param is NULL %s:%d \n", __FUNCTION__, __LINE__);
		return -1;
	
	}
		
	err = ioctl(fd, VIDIOC_QBUF, buf_param);
	
	if (err < 0) 
	{
		printf("cannot VIDIOC_QBUF \n");
		return -1;
	}

	return 0;

}


int bsp_v4l2_stream_on(int fd, int mp_buf_flag)
{
	int video_type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
	int err = 0;
	
	err = ioctl(fd, VIDIOC_STREAMON, &video_type);
	
	if (err < 0) 
    {
		printf("cannot VIDIOC_STREAMON err: %d\n", err);
		return -1;
    }

	return 0;

}

int bsp_v4l2_stream_off(int fd, int mp_buf_flag)
{
	int video_type = (mp_buf_flag ? 
		V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE);
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

