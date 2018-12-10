/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: bsp_v4l2_cap.h 
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
#ifndef _BSP_V4L2_CAP_H_
#define _BSP_V4L2_CAP_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct bsp_v4l2_cap_buf {
	__u32 bytes;
	char *addr;
} bsp_v4l2_cap_buf;

typedef struct bsp_v4l2_param {
	__u32 xres;
	__u32 yres;
	__u32 pixelformat;
	__u32 fps;
} bsp_v4l2_param;

extern int bsp_v4l2_open_dev(const char *dev_path, int *mp_buf_flag);

extern int bsp_v4l2_try_setup(int fd, struct bsp_v4l2_param *val, 
	int mp_buf_flag);

extern int bsp_v4l2_req_buf(int fd, struct bsp_v4l2_cap_buf buf_arr[], 
	int buf_count, int mp_buf_flag);

extern int bsp_v4l2_get_frame(int fd, struct v4l2_buffer *buf_param, 
	int mp_buf_flag);

extern int bsp_v4l2_put_frame_buf(int fd, struct v4l2_buffer *buf_param);

extern int bsp_v4l2_stream_on(int fd, int mp_buf_flag);

extern int bsp_v4l2_stream_off(int fd, int mp_buf_flag);

extern int bsp_v4l2_close_dev(int fd);

extern void bsp_print_fps(const char *fsp_dsc, long *fps, 
	long *pre_time, long *curr_time);

#ifdef __cplusplus
}
#endif

#endif /* ifndef _BSP_V4L2_CAP_H_.2018-8-1 23:20:50 folks */





