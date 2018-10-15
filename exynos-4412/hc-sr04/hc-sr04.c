#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define IOC_GET_US_TIME_GAP (0x01)
#define SPEED_OF_SONIC (0.34) /*0.34mm/us*/

int main(int argc, char *argv[])
{
	int fd, cnt, err;
	unsigned long time_gap_us;
	float distance = 0.0;

	cnt = 40;	
	fd = open("/dev/hc-sr04", O_RDWR | O_NONBLOCK);
	
	if (fd < 0) 
	{
		perror("open no device");
		return -1;
	}
	
	while (cnt--)
	{
		err = ioctl(fd, IOC_GET_US_TIME_GAP, &time_gap_us);

		if (err < 0)
		{
			printf("ioctl IOC_GET_US_TIME_GAP err %d \n", err);
			continue;
		}

		distance = (time_gap_us * SPEED_OF_SONIC) * 0.5;
		printf("distance: %f mm\n", distance);
	}

	close(fd);
	
    return 0;
}

