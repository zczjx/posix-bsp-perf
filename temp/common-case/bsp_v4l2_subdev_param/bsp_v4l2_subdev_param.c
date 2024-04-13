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
#include <linux/v4l2-subdev.h>

int main(int argc, char *argv[])
{
	int err;
	int fd;
	int i = 0;
	char *dev_path = NULL;
	int buf_mp_flag = 0;

	if(argc < 2)
	{
		printf("usage: bsp_v4l2_subdev_param [dev_path]\n");
		return -1;
	}

	dev_path = argv[1];
	fd = bsp_v4l2_subdev_open(dev_path);

	if(fd < 0)
		return fd;
	
	/*v4l2_subdev_mbus_code_enum test*/
	struct v4l2_subdev_mbus_code_enum mbus_code;
	memset(&mbus_code, 0, sizeof(struct v4l2_subdev_mbus_code_enum));

	for(i = 0; i < 32; i++)
	{
		mbus_code.index = i;
		err = ioctl(fd, VIDIOC_SUBDEV_ENUM_MBUS_CODE, &mbus_code);
	
		if (err < 0)
    	{
			break;
		}

		printf("--------enum format-----------\n");
		printf("[%s]: mbus_code.pad: %d \n", dev_path, mbus_code.pad);
		printf("[%s]: mbus_code.index: %d \n", dev_path, mbus_code.index);
		printf("[%s]: mbus_code.code: 0x%x \n", dev_path, mbus_code.code);
		printf("\n");
	}
	
	struct v4l2_subdev_format subdev_format;
	memset(&subdev_format, 0, sizeof(struct v4l2_subdev_format));
	err = ioctl(fd, VIDIOC_SUBDEV_G_FMT, &subdev_format);
		
	if (err < 0)
	{
		printf("VIDIOC_SUBDEV_G_FMT failed err: %d\n", err);
		return -1;
	}
		
	printf("[%s]: subdev_format.which: %d \n", dev_path, subdev_format.which);
	printf("[%s]: subdev_format.pad: %d \n", dev_path, subdev_format.pad);
	printf("[%s]: subdev_format.format.width: %d \n", 
		dev_path, subdev_format.format.width);
	printf("[%s]: subdev_format.format.height: %d \n", 
		dev_path, subdev_format.format.height);
	printf("[%s]: subdev_format.format.code: 0x%x \n", 
		dev_path, subdev_format.format.code);
	printf("[%s]: subdev_format.format.field: %d \n", 
		dev_path, subdev_format.format.field);
	printf("[%s]: subdev_format.format.colorspace: %d \n", 
		dev_path, subdev_format.format.colorspace);
	printf("[%s]: subdev_format.format.ycbcr_enc: %d \n", 
		dev_path, subdev_format.format.ycbcr_enc);
	printf("[%s]: subdev_format.format.quantization: %d \n", 
		dev_path, subdev_format.format.quantization);
	
	return 0;
}

