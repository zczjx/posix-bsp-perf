/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: bsp_perf_event.c 
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

#include "bsp_perf_event.h"

static int perf_cpu_cycles_open(int *fd);


int perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
							int cpu, int group_fd, unsigned long flags)
{
	int ret;

	ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
                   group_fd, flags);
	return ret;
}

static int perf_cpu_cycles_open(int *fd)
{
	struct perf_event_attr attr;
	int pfd = 0;

	memset(&attr, 0, sizeof(struct perf_event_attr));
	attr.type = PERF_TYPE_HARDWARE;
	attr.size = sizeof(struct perf_event_attr);
	attr.config = PERF_COUNT_HW_CPU_CYCLES;
	attr.read_format = (PERF_FORMAT_TOTAL_TIME_ENABLED | 
						PERF_FORMAT_TOTAL_TIME_RUNNING);
						
	pfd = perf_event_open(&attr, 0, -1, -1, 0);
	
	if (fd == -1) 
	{
		fprintf(stderr, "Error opening leader %llx\n", attr.config);
		*fd = -1;
		return -1;
	}

	*fd = pfd;
	
	return 0;

}


int perf_cpu_cycles_start(int *fd)
{
	int err = 0;
	
	if(NULL == fd)
	{
		printf("NULL pointer in %s:%d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	err = perf_cpu_cycles_open(fd);
	err = ioctl(*fd, PERF_EVENT_IOC_RESET);

	if(err < 0)
	{
		printf("PERF_EVENT_IOC_RESET failed !\n");
	}

	err = ioctl(*fd, PERF_EVENT_IOC_ENABLE, 0);

	if(err < 0)
	{
		printf("PERF_EVENT_IOC_ENABLE failed !\n");
	}

	return err;


}

int perf_cpu_cycles_result(int fd, unsigned long long *cpu_cycles)
{
	int err = 0;
	unsigned long long cnts[3];
	unsigned long long ret = 0;

	ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
	err = read(fd, cnts, sizeof(cnts));

	if(err < 0)
	{
		printf("read cpu_cycles failed!\n");
	}

	close(fd);

	ret = (unsigned long long) (float) cnts[0] * (float) cnts[1] 
		/ (float) cnts[2];
	*cpu_cycles = ret;

	return err;
}

