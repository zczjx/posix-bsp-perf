#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#define DEVICE_NAME				"pwm-buzzer"

#define PWM_IOCTL_SET_FREQ		1
#define PWM_IOCTL_STOP			0


int main(int argc, char *argv[])
{
	int fd, cnt, err;
	unsigned long freq = 0;

	if(argc < 2)
	{
		printf("usage: tiny4412_pwm_buzzer [freq]\n");
		return -1;
	}

	freq = atol(argv[1]);
	cnt = 5;	
	fd = open("/dev/pwm-buzzer", O_RDWR | O_NONBLOCK);
	
	if (fd < 0) 
	{
		perror("open no device");
		return -1;
	}

	err = ioctl(fd, PWM_IOCTL_SET_FREQ, freq);
	
	while (cnt--)
	{
		sleep(1);
	}

	err = ioctl(fd, PWM_IOCTL_STOP);
	close(fd);
	
    return 0;
}

