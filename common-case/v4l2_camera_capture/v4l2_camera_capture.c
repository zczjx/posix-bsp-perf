#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <time.h>

typedef struct cap_buf{
	int bytes;
	char *addr;
} cap_buf;

struct cap_buf v4l2_buf[4];
int buf_idx = 0;

int main(int argc, char *argv[])
{
	int err;
	int fd;
	int i = 0;
	int pts = 0;
	struct timespec tp;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	
	fd = open("/dev/video0", O_RDWR);
	if (fd < 0)
    {
		return -1;
	}
	/*capability test*/
	struct v4l2_capability v4l2_cap;
	memset(&v4l2_cap, 0, sizeof(struct v4l2_capability));
	err = ioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap);
	if (err < 0)
    {
		return -1;
	}
	
	printf("[/dev/video0]: v4l2_cap.driver: %s \n", v4l2_cap.driver);
	printf("[/dev/video0]: v4l2_cap.card: %s \n", v4l2_cap.card);
	printf("[/dev/video0]: v4l2_cap.bus_info: %s \n", v4l2_cap.bus_info);
	printf("[/dev/video0]: v4l2_cap.version: 0x%x \n", v4l2_cap.version);
	printf("[/dev/video0]: v4l2_cap.capabilities: 0x%x \n", v4l2_cap.capabilities);

	/*frame format test*/
	struct v4l2_fmtdesc fmt_dsc;
	memset(&fmt_dsc, 0, sizeof(struct v4l2_fmtdesc));
	fmt_dsc.index = 0;
	fmt_dsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	err = ioctl(fd, VIDIOC_ENUM_FMT, &fmt_dsc);
	if (err < 0)
    {
    	printf("[/dev/video0]:VIDIOC_ENUM_FMT fail \n");
		return -1;
	}
	struct v4l2_format  v4l2_fmt;
	memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
	v4l2_fmt.type = fmt_dsc.type;
    v4l2_fmt.fmt.pix.pixelformat = fmt_dsc.pixelformat;
    v4l2_fmt.fmt.pix.width       = 1920;
	v4l2_fmt.fmt.pix.height      = 1080;
    v4l2_fmt.fmt.pix.field       = V4L2_FIELD_ANY;
	err = ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt); 
    if (err) 
    {
		printf("[/dev/video0]:VIDIOC_S_FMT fail \n");
		return -1;      
    }
	
	err = ioctl(fd, VIDIOC_G_FMT, &v4l2_fmt);
	if (err < 0)
    {
    	printf("[/dev/video0]:VIDIOC_G_FMT fail \n");
		return -1;
	}
	printf("[/dev/video0]: v4l2_fmt.type: %d \n", v4l2_fmt.type);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.width: %d \n", v4l2_fmt.fmt.pix.width);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.height: %d \n", v4l2_fmt.fmt.pix.height);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.pixelformat: 0x%x \n", v4l2_fmt.fmt.pix.pixelformat);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.field: %d \n", v4l2_fmt.fmt.pix.field);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.bytesperline: %d \n", v4l2_fmt.fmt.pix.bytesperline);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.sizeimage: %d \n", v4l2_fmt.fmt.pix.sizeimage);
	printf("[/dev/video0]: v4l2_fmt.fmt.pix.colorspace: %d \n", v4l2_fmt.fmt.pix.colorspace);

	
	struct v4l2_streamparm streamparm;

	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	err = ioctl(fd, VIDIOC_G_PARM, &streamparm);
		
	if (err) 
	{
		printf("[/dev/video0]:VIDIOC_G_PARM failed \n");
		return -1;			
	}
	
	printf("[/dev/video0]:streamparm.parm.capture.capability 0x%x \n", 
			streamparm.parm.capture.capability);
	printf("[/dev/video0]:streamparm.parm.capture.capturemode 0x%x \n", 
			streamparm.parm.capture.capturemode);
	printf("[/dev/video0]:streamparm.parm.capture.timeperframe.denominator: %d \n",
				streamparm.parm.capture.timeperframe.denominator);
	printf("[/dev/video0]:streamparm.parm.capture.timeperframe.numerator: %d \n",
				streamparm.parm.capture.timeperframe.numerator);
	

	/* init buf */
	struct v4l2_buffer v4l2_buf_param;
	struct v4l2_requestbuffers req_bufs;
	
	memset(&req_bufs, 0, sizeof(struct v4l2_requestbuffers));
    req_bufs.count = 8;
    req_bufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req_bufs.memory = V4L2_MEMORY_MMAP;
    err = ioctl(fd, VIDIOC_REQBUFS, &req_bufs);
    if (err) 
    {
    	printf("req buf error!\n");
        return -1;        
    }

	if(v4l2_cap.capabilities & V4L2_CAP_STREAMING)
	{
		for(i = 0; i < 4; i++)
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
			v4l2_buf[i].bytes = v4l2_buf_param.length;
			v4l2_buf[i].addr = mmap(0 , v4l2_buf[i].bytes, 
								PROT_READ, MAP_SHARED, fd, 
								v4l2_buf_param.m.offset);
			err = ioctl(fd, VIDIOC_QBUF, &v4l2_buf_param);
			if (err) 
            {
            	printf("cannot VIDIOC_QBUF in mmap!\n");
        	    return -1;
        	}

		}
	}

	/* start stream capture*/
	struct pollfd fd_set[1];
	int video_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	err = ioctl(fd, VIDIOC_STREAMON, &video_type);
	if (err < 0) 
    {
		printf("cannot VIDIOC_STREAMON \n");
		return -1;
    }

	while(++pts <= 50000)
	{
		clock_gettime(CLOCK_MONOTONIC, &tp);
		curr_time = tp.tv_sec;
		//pre_time = curr_time - 1;
		fps++;
		if((curr_time - pre_time) >= 1)
		{
			printf("current fps is %d \n", fps);
			pre_time = curr_time;
			fps = 0;
		}
	
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
		
		buf_idx = v4l2_buf_param.index;

		printf("\n");
		printf("---------------------------------------------------------\n");
		printf("v4l2_buf_param.index : %d\n", v4l2_buf_param.index);
		printf("v4l2_buf_param.type : %d\n", v4l2_buf_param.type);
		printf("v4l2_buf_param.bytesused : %d\n", v4l2_buf_param.bytesused);
		printf("v4l2_buf_param.flags : 0x%x\n", v4l2_buf_param.flags);
		printf("v4l2_buf_param.field : %d\n", v4l2_buf_param.field);
		printf("v4l2_buf_param.timestamp.tv_sec : %lld\n", v4l2_buf_param.timestamp.tv_sec);
		printf("v4l2_buf_param.timestamp.tv_sec : %lld\n", v4l2_buf_param.timestamp.tv_sec);
		printf("v4l2_buf_param.timecode.type : %d\n", v4l2_buf_param.timecode.type);
		printf("v4l2_buf_param.timecode.flags : %d\n", v4l2_buf_param.timecode.flags);
		printf("v4l2_buf_param.timecode.frames : %d\n", v4l2_buf_param.timecode.frames);
		printf("v4l2_buf_param.timecode.seconds : %d\n", v4l2_buf_param.timecode.seconds);
		printf("v4l2_buf_param.timecode.minutes : %d\n", v4l2_buf_param.timecode.minutes);
		printf("v4l2_buf_param.timecode.hours : %d\n", v4l2_buf_param.timecode.hours);
		printf("v4l2_buf_param.timecode.userbits[0] : %d\n", v4l2_buf_param.timecode.userbits[0]);
		printf("v4l2_buf_param.timecode.userbits[1] : %d\n", v4l2_buf_param.timecode.userbits[1]);
		printf("v4l2_buf_param.timecode.userbits[2] : %d\n", v4l2_buf_param.timecode.userbits[2]);
		printf("v4l2_buf_param.timecode.userbits[3] : %d\n", v4l2_buf_param.timecode.userbits[3]);
		printf("v4l2_buf_param.sequence : %d\n", v4l2_buf_param.sequence);
		printf("v4l2_buf_param.memory : %d\n", v4l2_buf_param.memory);
		printf("v4l2_buf_param.length : %d\n", v4l2_buf_param.length);
		printf("---------------------------------------------------------\n");
		printf("\n");
	


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
		
	}

	
	return 0;
}


