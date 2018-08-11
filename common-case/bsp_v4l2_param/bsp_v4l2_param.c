#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#define V4L2_MAX_CTRL (128)
#define V4L2_MAX_FMT (128)

int main(int argc, char *argv[])
{
	int err;
	int fd;
	int i = 0;
	char *dev_path = NULL;

	if(argc < 2)
	{
		printf("usage: bsp_v4l2_param [dev_path]\n");
		return -1;
	}

	dev_path = argv[1];
	fd = open(dev_path, O_RDWR);

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
	
	printf("[%s]: v4l2_cap.driver: %s \n", dev_path, v4l2_cap.driver);
	printf("[%s]: v4l2_cap.card: %s \n", dev_path, v4l2_cap.card);
	printf("[%s]: v4l2_cap.bus_info: %s \n", dev_path, v4l2_cap.bus_info);
	printf("[%s]: v4l2_cap.version: 0x%x \n", dev_path, v4l2_cap.version);
	printf("[%s]: v4l2_cap.capabilities: 0x%x \n", dev_path, v4l2_cap.capabilities);

	/*frame format test*/
	struct v4l2_fmtdesc fmt_dsc;
	memset(&fmt_dsc, 0, sizeof(struct v4l2_fmtdesc));

	for(i = 0; i < V4L2_MAX_FMT; i++)
	{
		fmt_dsc.index = i;
		fmt_dsc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		err = ioctl(fd, VIDIOC_ENUM_FMT, &fmt_dsc);
		
		if (err < 0)
    		break;

		printf("\n");
		printf("[%s]: fmt_dsc.index: %d \n", dev_path, fmt_dsc.index);
		printf("[%s]: fmt_dsc.type: 0x%x \n", dev_path, fmt_dsc.type);
		printf("[%s]: fmt_dsc.flags: 0x%x \n", dev_path, fmt_dsc.flags);
		printf("[%s]: fmt_dsc.description: %s \n", dev_path, fmt_dsc.description);
		printf("[%s]: fmt_dsc.pixelformat: 0x%x \n", dev_path, fmt_dsc.pixelformat);
		printf("\n");
	}
	
	struct v4l2_format  v4l2_fmt;
	memset(&v4l2_fmt, 0, sizeof(struct v4l2_format));
	v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    v4l2_fmt.fmt.pix.width       = 1000;
	v4l2_fmt.fmt.pix.height      = 1000;
    v4l2_fmt.fmt.pix.field       = V4L2_FIELD_ANY;
	err = ioctl(fd, VIDIOC_S_FMT, &v4l2_fmt); 
	
    if (err) 
    {
		printf("[%s]:VIDIOC_S_FMT fail \n");
		return -1;      
    }
	
	err = ioctl(fd, VIDIOC_G_FMT, &v4l2_fmt);
	
	if (err < 0)
    {
    	printf("[%s]:VIDIOC_G_FMT fail \n");
		return -1;
	}

	printf("\n");
	printf("[%s]: v4l2_fmt.type: %d \n", dev_path, v4l2_fmt.type);
	printf("[%s]: v4l2_fmt.fmt.pix.width: %d \n", dev_path, v4l2_fmt.fmt.pix.width);
	printf("[%s]: v4l2_fmt.fmt.pix.height: %d \n", dev_path, v4l2_fmt.fmt.pix.height);
	printf("[%s]: v4l2_fmt.fmt.pix.pixelformat: 0x%x \n", dev_path, v4l2_fmt.fmt.pix.pixelformat);
	printf("[%s]: v4l2_fmt.fmt.pix.field: %d \n", dev_path, v4l2_fmt.fmt.pix.field);
	printf("[%s]: v4l2_fmt.fmt.pix.bytesperline: %d \n", dev_path, v4l2_fmt.fmt.pix.bytesperline);
	printf("[%s]: v4l2_fmt.fmt.pix.sizeimage: %d \n", dev_path, v4l2_fmt.fmt.pix.sizeimage);
	printf("[%s]: v4l2_fmt.fmt.pix.colorspace: %d \n", dev_path, v4l2_fmt.fmt.pix.colorspace);
	printf("\n");

	struct v4l2_streamparm streamparm;

	streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(fd, VIDIOC_G_PARM, &streamparm);
	
	if (err) 
    {
		printf("[%s]:VIDIOC_G_PARM failed \n");
		return -1;          
    }

	printf("[%s]:streamparm.parm.capture.capability 0x%x \n", 
			dev_path, streamparm.parm.capture.capability);
	printf("[%s]:streamparm.parm.capture.capturemode 0x%x \n", 
			dev_path, streamparm.parm.capture.capturemode);
	printf("[%s]:streamparm.parm.capture.timeperframe.denominator: %d \n",
			dev_path, streamparm.parm.capture.timeperframe.denominator);
	printf("[%s]:streamparm.parm.capture.timeperframe.numerator: %d \n",
			dev_path, streamparm.parm.capture.timeperframe.numerator);



	/* v4l2_queryctrl test */

	struct v4l2_queryctrl qctrl;
	memset(&qctrl, 0, sizeof(struct v4l2_queryctrl));
	for(i = 0; i < V4L2_MAX_CTRL; i++)
	{
		qctrl.id = V4L2_CID_BASE + i;
		err = ioctl(fd, VIDIOC_QUERYCTRL, &qctrl);
		
		if(0 == err)
		{
			printf("\n");
			printf("[%s]: qctrl.id: 0x%x \n", dev_path, qctrl.id);
			printf("[%s]: qctrl.type: %d \n", dev_path, qctrl.type);
			printf("[%s]: qctrl.name: %s \n", dev_path, qctrl.name);
			printf("[%s]: qctrl.minimum: %d \n", dev_path, qctrl.minimum);
			printf("[%s]: qctrl.maximum: %d \n", dev_path, qctrl.maximum);
			printf("[%s]: qctrl.step: %d \n", dev_path, qctrl.step);
			printf("[%s]: qctrl.default_value: %d \n", dev_path, qctrl.default_value);
			printf("[%s]: qctrl.flags: 0x%x \n", dev_path, qctrl.flags);
			printf("\n");
		}
		
	}
	
	return 0;
}




