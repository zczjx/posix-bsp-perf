#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	int err;
	int fd;
	char *dev_path = NULL;
	struct fb_var_screeninfo fb_var_info;
	struct fb_fix_screeninfo fb_fix_info;
	struct fb_cmap color_space[16];

	if(argc < 2)
	{
		printf("usage: bsp_fb_param [dev_path]\n");
		return -1;
	}

	dev_path = argv[1];
	fd = open(dev_path, O_RDWR);
	
	if (fd < 0)
    {
		return -1;
	}
	
	err = ioctl(fd, FBIOGET_VSCREENINFO, &fb_var_info);
	
	if (err < 0)
    {
		return -1;
	}

	printf("------ variable info of fb -------------\n");
	printf("\n");
	printf("[%s]: fb_var_info.xres: %d \n", dev_path, fb_var_info.xres);
	printf("[%s]: fb_var_info.yres: %d \n", dev_path, fb_var_info.yres);
	printf("[%s]: fb_var_info.xres_virtual: %d \n", dev_path, fb_var_info.xres_virtual);
	printf("[%s]: fb_var_info.yres_virtual: %d \n", dev_path, fb_var_info.yres_virtual);
	printf("[%s]: fb_var_info.xoffset: %d \n", dev_path, fb_var_info.xoffset);
	printf("[%s]: fb_var_info.yoffset: %d \n", dev_path, fb_var_info.yoffset);
	printf("[%s]: fb_var_info.bits_per_pixel: %d \n", dev_path, fb_var_info.bits_per_pixel);
	printf("[%s]: fb_var_info.grayscale: %d \n", dev_path, fb_var_info.grayscale);

	printf("[%s]: fb_var_info.red.offset: %d \n", dev_path, fb_var_info.red.offset);
	printf("[%s]: fb_var_info.red.length: %d \n", dev_path, fb_var_info.red.length);
	printf("[%s]: fb_var_info.red.msb_right: %d \n", dev_path, fb_var_info.red.msb_right);

	printf("[%s]: fb_var_info.green.offset: %d \n", dev_path, fb_var_info.green.offset);
	printf("[%s]: fb_var_info.green.length: %d \n", dev_path, fb_var_info.green.length);
	printf("[%s]: fb_var_info.green.msb_right: %d \n", dev_path, fb_var_info.green.msb_right);

	printf("[%s]: fb_var_info.blue.offset: %d \n", dev_path, fb_var_info.blue.offset);
	printf("[%s]: fb_var_info.blue.length: %d \n", dev_path, fb_var_info.blue.length);
	printf("[%s]: fb_var_info.blue.msb_right: %d \n", dev_path, fb_var_info.blue.msb_right);

	printf("[%s]: fb_var_info.transp.offset: %d \n", dev_path, fb_var_info.transp.offset);
	printf("[%s]: fb_var_info.transp.length: %d \n", dev_path, fb_var_info.transp.length);
	printf("[%s]: fb_var_info.transp.msb_right: %d \n", dev_path, fb_var_info.transp.msb_right);

	printf("[%s]: fb_var_info.nonstd: %d \n", dev_path, fb_var_info.nonstd);
	printf("[%s]: fb_var_info.activate: %d \n", dev_path, fb_var_info.activate);
	printf("[%s]: fb_var_info.height: %d \n", dev_path, fb_var_info.height);
	printf("[%s]: fb_var_info.width: %d \n", dev_path, fb_var_info.width);
	printf("[%s]: fb_var_info.accel_flags: %d \n", dev_path, fb_var_info.accel_flags);

	printf("[%s]: fb_var_info.pixclock: %d \n", dev_path, fb_var_info.pixclock);
	printf("[%s]: fb_var_info.left_margin: %d \n", dev_path, fb_var_info.left_margin);
	printf("[%s]: fb_var_info.right_margin: %d \n", dev_path, fb_var_info.right_margin);
	printf("[%s]: fb_var_info.upper_margin: %d \n", dev_path, fb_var_info.upper_margin);

	printf("[%s]: fb_var_info.lower_margin: %d \n", dev_path, fb_var_info.lower_margin);
	printf("[%s]: fb_var_info.hsync_len: %d \n", dev_path, fb_var_info.hsync_len);
	printf("[%s]: fb_var_info.vsync_len: %d \n", dev_path, fb_var_info.vsync_len);
	printf("[%s]: fb_var_info.sync: %d \n", dev_path, fb_var_info.sync);
	printf("[%s]: fb_var_info.vmode: %d \n", dev_path, fb_var_info.vmode);
	printf("[%s]: fb_var_info.rotate: %d \n", dev_path, fb_var_info.rotate);
	printf("\n");

	printf("------ fixed info of fb -------------\n");
	printf("\n");

	err = ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix_info);
	
	if (err < 0)
    {
		return -1;
	}
	
	printf("[%s]: fb_fix_info.id: %s \n", dev_path, fb_fix_info.id);
	printf("[%s]: fb_fix_info.smem_start: 0x%x \n", dev_path, fb_fix_info.smem_start);
	printf("[%s]: fb_fix_info.smem_len: %d \n", dev_path, fb_fix_info.smem_len);

	printf("[%s]: fb_fix_info.type: %d \n", dev_path, fb_fix_info.type);
	printf("[%s]: fb_fix_info.type_aux: %d \n", dev_path, fb_fix_info.type_aux);	

	printf("[%s]: fb_fix_info.visual: %d \n", dev_path, fb_fix_info.visual);
	printf("[%s]: fb_fix_info.xpanstep: %d \n", dev_path, fb_fix_info.xpanstep);	
	printf("[%s]: fb_fix_info.ypanstep: %d \n", dev_path, fb_fix_info.ypanstep);
	printf("[%s]: fb_fix_info.ywrapstep: %d \n", dev_path, fb_fix_info.ywrapstep);	

	printf("[%s]: fb_fix_info.line_length: %d \n", dev_path, fb_fix_info.line_length);
	printf("[%s]: fb_fix_info.mmio_start: 0x%X \n", dev_path, fb_fix_info.mmio_start);	
	printf("[%s]: fb_fix_info.mmio_len: %d \n", dev_path, fb_fix_info.mmio_len);
	printf("[%s]: fb_fix_info.accel: %d \n", dev_path, fb_fix_info.accel);	
	printf("\n");

	err = ioctl(fd, FBIOGETCMAP, color_space);
	if (err < 0)
    {
		printf("[%s]: FBIOGETCMAP fail \n", dev_path);
		return -1;
	}

	printf("[%s]: color_space.start: %d \n", dev_path, color_space[0].start);
	printf("[%s]: color_space.len: %d \n", dev_path, color_space[0].len);
	printf("[%s]: color_space.red: %d \n", dev_path, *color_space[0].red);
	printf("[%s]: color_space.green: %d \n", dev_path, *color_space[0].green);
	printf("[%s]: color_space.blue: %d \n", dev_path, *color_space[0].blue);
	printf("[%s]: color_space.transp: %d \n", dev_path, *color_space[0].transp);

	return 0;
	
}


