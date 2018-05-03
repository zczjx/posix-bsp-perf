#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

int main(int argc, char *argv[])
{
	int err;
	int fd;
	
	printf("[/dev/video0]: before open \n");
	fd = open("/dev/video0", O_RDWR);
	printf("[/dev/video0]: after open \n");
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
    v4l2_fmt.fmt.pix.width       = 1000;
	v4l2_fmt.fmt.pix.height      = 1000;
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

	
	/*v4l2_queryctrl test*/
#define V4L2_MAX_CTRL (128)
	int i = 0;
	struct v4l2_queryctrl qctrl;
	memset(&qctrl, 0, sizeof(struct v4l2_queryctrl));
	for(i = 0; i < V4L2_MAX_CTRL; i++)
	{
		qctrl.id = V4L2_CID_BASE + i;
		err = ioctl(fd, VIDIOC_QUERYCTRL, &qctrl);
		if(0 == err)
		{
			printf("\n");
			printf("[/dev/video0]: qctrl.id: 0x%x \n", qctrl.id);
			printf("[/dev/video0]: qctrl.type: %d \n", qctrl.type);
			printf("[/dev/video0]: qctrl.name: %s \n", qctrl.name);
			printf("[/dev/video0]: qctrl.minimum: %d \n", qctrl.minimum);
			printf("[/dev/video0]: qctrl.maximum: %d \n", qctrl.maximum);
			printf("[/dev/video0]: qctrl.step: %d \n", qctrl.step);
			printf("[/dev/video0]: qctrl.default_value: %d \n", qctrl.default_value);
			printf("[/dev/video0]: qctrl.flags: 0x%x \n", qctrl.flags);
			printf("\n");
		}
	}
	return 0;
}




