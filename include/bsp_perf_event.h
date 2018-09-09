/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: bsp_perf_event.h 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2018-9-9    
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
#ifndef _BSP_PERF_EVENT_H_
#define _BSP_PERF_EVENT_H_

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
#include <asm/unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>

extern int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, 
									int cpu, int group_fd, unsigned long flags);

extern int perf_cpu_cycles_start(int *fd);

extern int perf_cpu_cycles_result(int fd, unsigned long long *cpu_cycles);


#ifdef __cplusplus
}
#endif

#endif /* ifndef _BSP_PERF_EVENT_H_.2018-9-9 20:06:44 folks */

