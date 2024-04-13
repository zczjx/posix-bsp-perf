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

	vfd = open(dev_path, O_RDWR | O_NONBLOCK);
	
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
	enum v4l2_buf_type buf_type)
{
	struct v4l2_format v4l2_fmt;
	struct v4l2_streamparm streamparm;
	int i, err = 0;

	if(NULL == val)
	{
		printf("%s NULL ptr val \n", __FUNCTION__);
		return -1;

	}
	
	memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
	v4l2_fmt.type = buf_type;
	
	if((V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type)
	|| (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == buf_type))
	{
		v4l2_fmt.fmt.pix_mp.pixelformat = val->pixelformat;
    	v4l2_fmt.fmt.pix_mp.width       = val->xres;
		v4l2_fmt.fmt.pix_mp.height      = val->yres;
    	v4l2_fmt.fmt.pix_mp.field       = V4L2_FIELD_NONE;

		for(i = 0; i < val->planes_num; i++)
		{
			v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage = val->req_buf_size[i];
		}
	
		v4l2_fmt.fmt.pix_mp.num_planes = val->planes_num;
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
		printf("VIDIOC_S_FMT fail [%s] line: [%d]\n", __func__, __LINE__);
	
	}

	
	err = ioctl(fd, VIDIOC_G_FMT, &v4l2_fmt);

	if (err < 0)
	{
		printf("VIDIOC_G_FMT fail err: %d [%s] line: [%d]\n", err, __func__, __LINE__);
	}

	if((V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type)
	|| (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == buf_type))
	{
		val->pixelformat = v4l2_fmt.fmt.pix_mp.pixelformat;
    	val->xres = v4l2_fmt.fmt.pix_mp.width;
		val->yres = v4l2_fmt.fmt.pix_mp.height;
		val->planes_num = v4l2_fmt.fmt.pix_mp.num_planes;
		for(i = 0; i < val->planes_num; i++)
		{
			val->req_buf_size[i] = v4l2_fmt.fmt.pix_mp.plane_fmt[i].sizeimage ;
		}
	}
	else
	{
		val->pixelformat = v4l2_fmt.fmt.pix.pixelformat;
		val->xres = v4l2_fmt.fmt.pix.width;
		val->yres = v4l2_fmt.fmt.pix.height;
	}

	streamparm.type = buf_type;
	err = ioctl(fd, VIDIOC_G_PARM, &streamparm);
		
	if (0 == err) 
	{
		val->fps = (__u32) (streamparm.parm.capture.timeperframe.denominator
				/ streamparm.parm.capture.timeperframe.numerator);
	}
	else
	{
		printf("VIDIOC_G_PARM failed [%s] line: [%d]\n", __func__, __LINE__);
		val->fps = 15;
	}
	
	return 0;
	
}

int bsp_v4l2_req_buf(int fd, struct bsp_v4l2_buf buf_arr[], 
	int buf_count, enum v4l2_buf_type buf_type, __u32 planes_num)
{
	struct v4l2_buffer v4l2_buf_param;
	struct v4l2_requestbuffers req_bufs;
	struct v4l2_plane *mplane = NULL;
	int err, i, j = 0;

	if(NULL == buf_arr)
	{
		printf("%s NULL ptr buf_arr \n", __FUNCTION__);
		return -1;

	}

	/* init buf */
	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
    req_bufs.count = buf_count;
    req_bufs.type = buf_type;
    req_bufs.memory = V4L2_MEMORY_MMAP;
    err = ioctl(fd, VIDIOC_REQBUFS, &req_bufs);

    if (err < 0) 
    {
    	printf("req buf err: %d [%s] line: %d\n", err, __func__, __LINE__);
        return -1;        
    }

	if(req_bufs.count > buf_count)
	{
		printf("req_bufs.count > buf_count req_bufs.count: %d [%s] line: %d\n",
				req_bufs.count, __func__, __LINE__);
        return -1;
	}

	if((V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type)
	|| (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == buf_type))
	{
		mplane = malloc(planes_num * sizeof(struct v4l2_plane));
		memset(mplane, 0x00, planes_num * sizeof(struct v4l2_plane));
	
		for(i = 0; i < req_bufs.count; i++)
		{
			memset(&v4l2_buf_param, 0, sizeof(struct v4l2_buffer));
			v4l2_buf_param.index = i;
        	v4l2_buf_param.type = buf_type;
        	v4l2_buf_param.memory = V4L2_MEMORY_MMAP;
			v4l2_buf_param.m.planes = mplane;
			v4l2_buf_param.length = planes_num;
			err = ioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf_param);
			
			if (err) 
			{
				printf("cannot mmap v4l2 buf err: %d\n", err);
				return -1;
        	}

			buf_arr[i].planes_num = planes_num;

			for(j = 0; j < planes_num; j++)
			{
				buf_arr[i].bytes[j] = v4l2_buf_param.m.planes[j].length;
				buf_arr[i].addr[j] = mmap(0 , v4l2_buf_param.m.planes[j].length, 
							PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
							v4l2_buf_param.m.planes[j].m.mem_offset);
			}
		}

		if(NULL != mplane)
		{
			free(mplane);
			mplane = NULL;
		}

	}
	else
	{
		for(i = 0; i < req_bufs.count; i++)
		{
			memset(&v4l2_buf_param, 0, sizeof(struct v4l2_buffer));
			v4l2_buf_param.index = i;
        	v4l2_buf_param.type = buf_type;
        	v4l2_buf_param.memory = V4L2_MEMORY_MMAP;
		
			err = ioctl(fd, VIDIOC_QUERYBUF, &v4l2_buf_param);
			
        	if (err) 
			{
				printf("cannot mmap v4l2 buf err: %d\n", err);
				return -1;
        	}

			buf_arr[i].planes_num = 0;
			buf_arr[i].bytes[0] = v4l2_buf_param.length;
			buf_arr[i].addr[0] = mmap(0 , v4l2_buf_param.length, 
									PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
									v4l2_buf_param.m.offset);
		}
	}
	
	printf("bsp_v4l2_req_buf OK [%s] [%d]\n", __func__, __LINE__);
	return req_bufs.count;
}

int bsp_v4l2_get_frame_buf(int fd, struct v4l2_buffer *buf_param, 
	enum v4l2_buf_type buf_type, __u32 planes_num)
{
	int err = 0;

	if(NULL == buf_param)
	{
		printf("buf_param is NULL %s:%d \n", __FUNCTION__, __LINE__);
		return -1;
	
	}

    buf_param->type = buf_type;
    buf_param->memory = V4L2_MEMORY_MMAP;

	if((V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE == buf_type)
	|| (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == buf_type))
	{
		if((buf_param->length != planes_num)
		|| (NULL == buf_param->m.planes))
		{
			printf("[%s]:[%d] please alloc mplanes in V4L2 mplane\n", __FUNCTION__, __LINE__);
			return -1;
		}
	}

    err = ioctl(fd, VIDIOC_DQBUF, buf_param);
		
	if (err < 0) 
	{
		printf("VIDIOC_DQBUF func: err: %d [%s]:[%d]\n", err, __FUNCTION__, __LINE__);
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
		printf("VIDIOC_QBUF func: err: %d [%s]:[%d]\n", err, __FUNCTION__, __LINE__);
		return -1;
	}

	return 0;

}


int bsp_v4l2_stream_on(int fd, enum v4l2_buf_type buf_type)
{
	int video_type = buf_type;
	int err = 0;
	
	err = ioctl(fd, VIDIOC_STREAMON, &video_type);
	
	if (err < 0) 
    {
		printf("cannot VIDIOC_STREAMON err: %d\n", err);
		return -1;
    }

	return 0;

}

int bsp_v4l2_stream_off(int fd, enum v4l2_buf_type buf_type)
{
	int video_type = buf_type;
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

