/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: bsp_fb.h 
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
#ifndef _BSP_FB_H_
#define _BSP_FB_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include <linux/fb.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

typedef struct bsp_fb_var_attr {
	__u32 xres;
	__u32 yres;
	__u32 bits_per_pixel;
	__u32 red_offset;
	__u32 green_offset;
	__u32 blue_offset;
	__u32 transp_offset;
	__u32 color_bits_len;
	unsigned char *fbmem;
	struct fb_var_screeninfo fb_var_info;
} bsp_fb_var_attr;

typedef struct bsp_fb_fix_attr {
	__u32 bytes_per_line;
	__u32 fb_buf_bytes;
	struct fb_fix_screeninfo fb_fix_info;
} bsp_fb_fix_attr;

typedef struct rgb_frame {
	__u32 xres;
	__u32 yres;
	__u32 bytes_per_line;
	__u32 bits_per_pixel;
	__u32 bytes;
	char *addr;
} rgb_frame;


extern int bsp_fb_open_dev(const char *dev_path, 
	struct bsp_fb_var_attr *var_attr, struct bsp_fb_fix_attr *fix_attr);

extern int bsp_fb_try_setup(int fd, struct bsp_fb_var_attr *var_attr);

extern int bsp_fb_flush(int fd, 
	struct bsp_fb_var_attr *var_attr, struct bsp_fb_fix_attr *fix_attr,
	struct rgb_frame *frame);

extern int bsp_fb_close_dev(int fd);


#ifdef __cplusplus
}
#endif

#endif /* ifndef _BSP_FB_H_.2018-8-3 22:23:01 folks */






