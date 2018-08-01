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
	struct fb_var_screeninfo fb_var_info;
	struct fb_fix_screeninfo fb_fix_info;
	struct fb_cmap color_space[16];

	fd = open("/dev/fb0", O_RDWR);
	if (fd < 0)
    {
		return -1;
	}
	err = ioctl(fd, FBIOGET_VSCREENINFO, &fb_var_info);
	if (err < 0)
    {
		return -1;
	}
	printf("[/dev/fb0]: fb_var_info.xres: %d \n", fb_var_info.xres);
	printf("[/dev/fb0]: fb_var_info.yres: %d \n", fb_var_info.yres);
	printf("[/dev/fb0]: fb_var_info.xres_virtual: %d \n", fb_var_info.xres_virtual);
	printf("[/dev/fb0]: fb_var_info.yres_virtual: %d \n", fb_var_info.yres_virtual);
	printf("[/dev/fb0]: fb_var_info.xoffset: %d \n", fb_var_info.xoffset);
	printf("[/dev/fb0]: fb_var_info.yoffset: %d \n", fb_var_info.yoffset);
	printf("[/dev/fb0]: fb_var_info.bits_per_pixel: %d \n", fb_var_info.bits_per_pixel);
	printf("[/dev/fb0]: fb_var_info.grayscale: %d \n", fb_var_info.grayscale);

	printf("[/dev/fb0]: fb_var_info.red.offset: %d \n", fb_var_info.red.offset);
	printf("[/dev/fb0]: fb_var_info.red.length: %d \n", fb_var_info.red.length);
	printf("[/dev/fb0]: fb_var_info.red.msb_right: %d \n", fb_var_info.red.msb_right);

	printf("[/dev/fb0]: fb_var_info.green.offset: %d \n", fb_var_info.green.offset);
	printf("[/dev/fb0]: fb_var_info.green.length: %d \n", fb_var_info.green.length);
	printf("[/dev/fb0]: fb_var_info.green.msb_right: %d \n", fb_var_info.green.msb_right);

	printf("[/dev/fb0]: fb_var_info.blue.offset: %d \n", fb_var_info.blue.offset);
	printf("[/dev/fb0]: fb_var_info.blue.length: %d \n", fb_var_info.blue.length);
	printf("[/dev/fb0]: fb_var_info.blue.msb_right: %d \n", fb_var_info.blue.msb_right);

	printf("[/dev/fb0]: fb_var_info.transp.offset: %d \n", fb_var_info.transp.offset);
	printf("[/dev/fb0]: fb_var_info.transp.length: %d \n", fb_var_info.transp.length);
	printf("[/dev/fb0]: fb_var_info.transp.msb_right: %d \n", fb_var_info.transp.msb_right);

	printf("[/dev/fb0]: fb_var_info.nonstd: %d \n", fb_var_info.nonstd);
	printf("[/dev/fb0]: fb_var_info.activate: %d \n", fb_var_info.activate);
	printf("[/dev/fb0]: fb_var_info.height: %d \n", fb_var_info.height);
	printf("[/dev/fb0]: fb_var_info.width: %d \n", fb_var_info.width);
	printf("[/dev/fb0]: fb_var_info.accel_flags: %d \n", fb_var_info.accel_flags);

	printf("[/dev/fb0]: fb_var_info.pixclock: %d \n", fb_var_info.pixclock);
	printf("[/dev/fb0]: fb_var_info.left_margin: %d \n", fb_var_info.left_margin);
	printf("[/dev/fb0]: fb_var_info.right_margin: %d \n", fb_var_info.right_margin);
	printf("[/dev/fb0]: fb_var_info.upper_margin: %d \n", fb_var_info.upper_margin);

	printf("[/dev/fb0]: fb_var_info.lower_margin: %d \n", fb_var_info.lower_margin);
	printf("[/dev/fb0]: fb_var_info.hsync_len: %d \n", fb_var_info.hsync_len);
	printf("[/dev/fb0]: fb_var_info.vsync_len: %d \n", fb_var_info.vsync_len);
	printf("[/dev/fb0]: fb_var_info.sync: %d \n", fb_var_info.sync);
	printf("[/dev/fb0]: fb_var_info.vmode: %d \n", fb_var_info.vmode);
	printf("[/dev/fb0]: fb_var_info.rotate: %d \n", fb_var_info.rotate);


	

	err = ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix_info);
	if (err < 0)
    {
		return -1;
	}
	
	printf("[/dev/fb0]: fb_fix_info.id: %s \n", fb_fix_info.id);
	printf("[/dev/fb0]: fb_fix_info.smem_start: 0x%x \n", fb_fix_info.smem_start);
	printf("[/dev/fb0]: fb_fix_info.smem_len: %d \n", fb_fix_info.smem_len);

	printf("[/dev/fb0]: fb_fix_info.type: %d \n", fb_fix_info.type);
	printf("[/dev/fb0]: fb_fix_info.type_aux: %d \n", fb_fix_info.type_aux);	

	printf("[/dev/fb0]: fb_fix_info.visual: %d \n", fb_fix_info.visual);
	printf("[/dev/fb0]: fb_fix_info.xpanstep: %d \n", fb_fix_info.xpanstep);	
	printf("[/dev/fb0]: fb_fix_info.ypanstep: %d \n", fb_fix_info.ypanstep);
	printf("[/dev/fb0]: fb_fix_info.ywrapstep: %d \n", fb_fix_info.ywrapstep);	

	printf("[/dev/fb0]: fb_fix_info.line_length: %d \n", fb_fix_info.line_length);
	printf("[/dev/fb0]: fb_fix_info.mmio_start: 0x%X \n", fb_fix_info.mmio_start);	
	printf("[/dev/fb0]: fb_fix_info.mmio_len: %d \n", fb_fix_info.mmio_len);
	printf("[/dev/fb0]: fb_fix_info.accel: %d \n", fb_fix_info.accel);	

	err = ioctl(fd, FBIOGETCMAP, color_space);
	if (err < 0)
    {
		printf("[/dev/fb0]: FBIOGETCMAP fail \n");
		return -1;
	}

	printf("[/dev/fb0]: color_space.start: %d \n", color_space[0].start);
	printf("[/dev/fb0]: color_space.len: %d \n", color_space[0].len);
	printf("[/dev/fb0]: color_space.red: %d \n", *color_space[0].red);
	printf("[/dev/fb0]: color_space.green: %d \n", *color_space[0].green);
	printf("[/dev/fb0]: color_space.blue: %d \n", *color_space[0].blue);
	printf("[/dev/fb0]: color_space.transp: %d \n", *color_space[0].transp);

	return 0;
}


