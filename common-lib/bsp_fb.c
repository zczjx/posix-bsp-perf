/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: bsp_fb.c 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2018-8-3    
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
#include "bsp_fb.h"

int bsp_fb_open_dev(const char *dev_path, 
	struct bsp_fb_var_attr *var_attr, struct bsp_fb_fix_attr *fix_attr)
{
	int fd, err;

	if((NULL == dev_path)
	|| (NULL == var_attr)
	|| (NULL == fix_attr))
	{
		printf("%s NULL ptr dev_path \n", __FUNCTION__);
		return -1;

	}

	fd = open(dev_path, O_RDWR);
	
	if (fd < 0)
    {
    	fprintf(stderr, "could not open %s\n", dev_path);
		return -1;
	}
	
	memset(&var_attr->fb_var_info, 0, sizeof(struct fb_var_screeninfo));
	memset(&fix_attr->fb_fix_info, 0, sizeof(struct fb_fix_screeninfo));

	err = ioctl(fd, FBIOGET_VSCREENINFO, &var_attr->fb_var_info);
	
	if (err < 0)
    {
    	printf("[/dev/fb0]: FBIOGET_VSCREENINFO fail \n");
		return -1;
	}

	var_attr->xres = var_attr->fb_var_info.xres;
	var_attr->yres = var_attr->fb_var_info.yres;
	var_attr->bits_per_pixel = var_attr->fb_var_info.bits_per_pixel;
	var_attr->red_offset = var_attr->fb_var_info.red.offset;
	var_attr->green_offset = var_attr->fb_var_info.green.offset;
	var_attr->blue_offset = var_attr->fb_var_info.blue.offset;
	var_attr->transp_offset = var_attr->fb_var_info.transp.offset;
	var_attr->color_bits_len = var_attr->fb_var_info.transp.length;
	
	err = ioctl(fd, FBIOGET_FSCREENINFO, &fix_attr->fb_fix_info);
	
	if (err < 0)
    {
    	printf("[/dev/fb0]: FBIOGET_FSCREENINFO fail \n");
		return -1;
	}

	fix_attr->bytes_per_line = fix_attr->fb_fix_info.line_length;
	fix_attr->fb_buf_bytes = fix_attr->fb_fix_info.smem_len;

	var_attr->fbmem = (unsigned char *)mmap(NULL, fix_attr->fb_buf_bytes,
											(PROT_READ | PROT_WRITE), MAP_SHARED, 
											fd, 0);
	return 0;

}

int bsp_fb_try_setup(int fd, struct bsp_fb_var_attr *var_attr)
{
	int err = 0;
	
	if(NULL == var_attr)
	{
		printf("%s NULL ptr dev_path \n", __FUNCTION__);
		return -1;
	}

	var_attr->fb_var_info.xres = var_attr->xres;
	var_attr->fb_var_info.yres = var_attr->yres;
	var_attr->fb_var_info.bits_per_pixel = var_attr->bits_per_pixel;
	var_attr->fb_var_info.red.offset = var_attr->red_offset;
	var_attr->fb_var_info.green.offset = var_attr->green_offset;
	var_attr->fb_var_info.blue.offset = var_attr->blue_offset;
	var_attr->fb_var_info.transp.offset = var_attr->transp_offset;
	var_attr->fb_var_info.transp.length = var_attr->color_bits_len;

	err = ioctl(fd, FBIOPUT_VSCREENINFO, &var_attr->fb_var_info);
	
	if (err < 0)
    {
    	printf("[/dev/fb0]: FBIOPUT_VSCREENINFO fail \n");
		return -1;
	}

	return err;
}

int bsp_fb_flush(int fd, 
	struct bsp_fb_var_attr *var_attr, struct bsp_fb_fix_attr *fix_attr,
	struct rgb_frame *frame)
{
	__u32 flush_xres;
	__u32 flush_yres;
	__u32 flush_bytes_per_line;
	__u32 i = 0;
	unsigned char *fb_pos = NULL;
	unsigned char *image_pos = NULL;

	if((NULL == var_attr)
	|| (NULL == fix_attr)
	|| (NULL == frame))
	{
		printf("%s NULL ptr dev_path \n", __FUNCTION__);
		return -1;

	}

	flush_xres = (var_attr->xres > frame->xres) ? frame->xres : var_attr->xres;
	flush_yres = (var_attr->yres > frame->yres) ? frame->yres : var_attr->yres;
	flush_bytes_per_line = (fix_attr->bytes_per_line > frame->bytes_per_line) ? 
				fix_attr->bytes_per_line : frame->bytes_per_line;

	fb_pos = var_attr->fbmem;
	image_pos = frame->addr;

	for(i = 0; i < flush_yres; i++)
	{
		memcpy(fb_pos, image_pos, flush_bytes_per_line);
		fb_pos += fix_attr->bytes_per_line;
		image_pos += frame->bytes_per_line;
	}

	return 0;

}

int bsp_fb_close_dev(int fd)
{

	close(fd);
	
	return 0;

}


