#include <stdio.h>
#include <sys/time.h>
#include "bsp_perf_event.h"


#define MEM_SIZE_256MB (256 << 20)
#define NUM_1M (1000000)

static float calculate_sdram_vitual_bw(unsigned int test_time_us);
static float calculate_sdram_phy_bw(unsigned long long cpu_cycles, 
	int cpu_freq);


int main(int argc, char *argv[])
{
    int i, j, fd, err = 0;
	unsigned long sum = 0;
	unsigned long *test_arr = NULL;
	struct timeval time1, time2;
	unsigned int us = 0;
	unsigned int data_bits_width = 0;
	unsigned long rw_cnt = 0;
	float phy_bw, vitual_membw = 0.0;
	unsigned long long cpu_cycles;
	int  freq = 0;

	if(argc < 2)
	{
		printf("usage: sdram_bandwidth [cpu_freq MHz]\n");
		return -1;
	}

	freq = atoi(argv[1]);
	data_bits_width = sizeof(unsigned long);
	rw_cnt = MEM_SIZE_256MB / data_bits_width;
	test_arr = (unsigned long *) malloc(MEM_SIZE_256MB + 0x10);

	if(NULL == test_arr)
	{
		printf("too large mem size, cannot test \n");
		return -1;
	}

	// align 16 byte
	test_arr = (unsigned long *)(((unsigned long)test_arr & ~0x1f) + 0x10);

	for(i = 0; i < rw_cnt; i++)
	{
		test_arr[i] = 0xaa;
	}

	printf("------------SDRAM read bandwidth test--------- \n");
	gettimeofday(&time1, NULL);
	perf_cpu_cycles_start(&fd);
	
	for(i = 0; i < rw_cnt; i++)
	{
		sum += test_arr[i];
	}

	perf_cpu_cycles_result(fd, &cpu_cycles);
	gettimeofday(&time2, NULL);
	us = time2.tv_sec * NUM_1M + time2.tv_usec
		- time1.tv_sec * NUM_1M - time1.tv_usec;
	printf("SDRAM read test time: %d us! \n", us);
	printf("SDRAM read cpu cycles: %llu\n", cpu_cycles);
	vitual_membw = calculate_sdram_vitual_bw(us);
	phy_bw = calculate_sdram_phy_bw(cpu_cycles, freq);
	printf("SDRAM read vitual bandwidth: %0.2f MB/s\n", vitual_membw);
	printf("SDRAM read phy bandwidth: %0.2f MB/s\n", phy_bw);

	printf("------------SDRAM write bandwidth test--------- \n");
	gettimeofday(&time1, NULL);
	perf_cpu_cycles_start(&fd);
	
	for(i = 0; i < rw_cnt; i++)
	{
		test_arr[i] = 0x55;
	}
	
	perf_cpu_cycles_result(fd, &cpu_cycles);
	gettimeofday(&time2, NULL);
	us = time2.tv_sec * NUM_1M + time2.tv_usec
		- time1.tv_sec * NUM_1M - time1.tv_usec;
	printf("SDRAM write test time: %d us! \n", us);
	printf("SDRAM write cpu cycles: %llu\n", cpu_cycles);
	vitual_membw = calculate_sdram_vitual_bw(us);
	phy_bw = calculate_sdram_phy_bw(cpu_cycles, freq);
	printf("SDRAM write vitual bandwidth: %0.2f MB/s\n", vitual_membw);
	printf("SDRAM write phy bandwidth: %0.2f MB/s\n", phy_bw);
 
    return 0;
}

static float calculate_sdram_vitual_bw(unsigned int test_time_us)
{
	float bytes_per_us = 0.0;
	float bytes_per_sec = 0.0;
	float mb_per_sec = 0.0;

	bytes_per_us = (float)MEM_SIZE_256MB / test_time_us;
	bytes_per_sec = bytes_per_us * NUM_1M;
	mb_per_sec = bytes_per_sec / (1024.0f * 1024.0f);
	
	return mb_per_sec;

}

static float calculate_sdram_phy_bw(unsigned long long cpu_cycles, 
	int cpu_freq)
{
	float time_us;
	float bytes_per_us = 0.0;
	float bytes_per_sec = 0.0;
	float mb_per_sec = 0.0;

	time_us = (float) (cpu_cycles / cpu_freq);
	bytes_per_us = (float)MEM_SIZE_256MB / time_us;
	bytes_per_sec = bytes_per_us * NUM_1M;
	mb_per_sec = bytes_per_sec / (1024.0f * 1024.0f);

	return mb_per_sec;
}


