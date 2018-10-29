/*******************************************************************************
* Copyright (C), 2000-2018,  Electronic Technology Co., Ltd.
*                
* @filename: main.c 
*                
* @author: Clarence.Zhou <zhou_chenz@163.com> 
*                
* @version:
*                
* @date: 2018-10-23    
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

#include <stdio.h>
#include "rc522.h"

int fd_spidev;
uint8_t bits = 8;
uint32_t speed = 1000000;
uint16_t delay;
static uint32_t mode;

unsigned char data1[16] = { 0x42,0x31,0x44,0x20,
							0x54,0x10,0x35,0x20,
							0x53,0x20,0x43,0x30,
							0x54,0x10,0x36,0x10 };
//M1卡的某一块写为如下格式，则该块为钱包，可接收扣款和充值命令
//4字节金额（低字节在前）＋4字节金额取反＋4字节金额＋1字节块地址＋1字节块地址取反＋1字节块地址＋1字节块地址取反 
unsigned char data2[4]  = {0,0,0,0x01};

unsigned char DefaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define CARD_CMD_READ_VAL	0
#define CARD_CMD_SET_VAL	1
#define CARD_CMD_ADD_VAL	2
#define CARD_CMD_DEC_VAL	3

const char *card_ops[] = {
	[CARD_CMD_READ_VAL] = "read value",
	[CARD_CMD_SET_VAL] = "set value",
	[CARD_CMD_ADD_VAL] = "add value",
	[CARD_CMD_DEC_VAL] = "dec value",
	NULL
};

static int pre_card_opr_flow()
{
	unsigned char g_ucTempbuf[20];
	int status, i;
	unsigned int temp;

	memset(g_ucTempbuf, 0x00, sizeof(g_ucTempbuf));
	status = PcdRequest(PICC_REQALL, g_ucTempbuf);//寻卡
		
	if (status != MI_OK)
	{    
		PcdReset();
		PcdAntennaOff();
		PcdAntennaOn();
		return status;
	}
	
	printf("\n");
	printf("type of nfc card: 0x");
		
	for(i = 0; i < 2; i++)
	{
		temp = g_ucTempbuf[i];
		printf("%02x", temp);		
	}

	printf("\n");
	status = PcdAnticoll(g_ucTempbuf);//防冲撞
		
	if (status != MI_OK)
	{
		printf("Card Anticoll Failed \n");
		return status;
	}

	printf("serial no：0x");	//超级终端显示,
		
	for(i = 0; i < 4; i++)
	{
		temp = g_ucTempbuf[i];
		printf("%2x",temp);
						
	}
	printf("\n");

	status = PcdSelect(g_ucTempbuf);//选定卡片
	
	if (status != MI_OK)
	{
		printf("Card Select Failed \n");
		return status;
	}

	status = PcdAuthState(PICC_AUTHENT1A, 1, DefaultKey, g_ucTempbuf);//验证卡片密码
	
    if (status != MI_OK)
	{
		printf("Card Auth key verify Failed \n");
		return status;
	}
	
	return MI_OK;

}

static int do_card_opr(int cmd)
{
	int ret = 0;
	int status,i;
	unsigned char g_ucTempbuf[20];
	int blk_nr = 0;	
	unsigned char temp;
	unsigned long val;
	unsigned char blk_val[16];
	unsigned char delta[4];
	unsigned char *paddr = NULL;

	memset(g_ucTempbuf, 0x00, sizeof(g_ucTempbuf));
	memset(blk_val, 0x00, sizeof(blk_val));
	memset(delta, 0x00, sizeof(delta));

	switch(cmd)
	{
		case CARD_CMD_READ_VAL:
			printf("enter blk nr to read\n");
			scanf("%d", &blk_nr);
			status = PcdRead(blk_nr, g_ucTempbuf);//读块

			if (status != MI_OK)
			{
				printf("blk_nr: %d read failed \n", blk_nr);
				return status;

			}
			
			printf("nfc read val:");	//超级终端显示
			printf("\n");
			printf("0x");
			
			for(i = 0; i < 4; i++)
			{
				temp = g_ucTempbuf[i];
				printf("%02x", temp);			
			}
			
			printf("\n");
			break;

		case CARD_CMD_SET_VAL:
			printf("enter blk nr to set\n");
			scanf("%d", &blk_nr);
			printf("enter val to set\n");
			scanf("%ul", &val);

			blk_val[3] = (unsigned char) (val & 0x000000ff);
			blk_val[2] = (unsigned char) ((val >> 8) & 0x000000ff);
			blk_val[1] = (unsigned char) ((val >> 16) & 0x000000ff);
			blk_val[0] = (unsigned char) ((val >> 24) & 0x000000ff);
			
			blk_val[7] = ~blk_val[3];
			blk_val[6] = ~blk_val[2];
			blk_val[5] = ~blk_val[1];
			blk_val[4] = ~blk_val[0];

			blk_val[11] = blk_val[3];
			blk_val[10] = blk_val[2];
			blk_val[9] = blk_val[1];
			blk_val[8] = blk_val[0];

			
			blk_val[15] = ~blk_nr;
			blk_val[14] = blk_nr;
			blk_val[13] = ~blk_nr;
			blk_val[12] = blk_nr;
			status = PcdWrite(blk_nr, blk_val);

			if (status != MI_OK)
			{
				printf("blk_nr: %d set failed \n", blk_nr);
				return status;

			}
			
			break;

		case CARD_CMD_ADD_VAL:
			printf("enter blk nr to add\n");
			scanf("%d", &blk_nr);
			printf("enter val to add\n");
			scanf("%ul", &val);
			delta[3] = (unsigned char) (val & 0x000000ff);
			delta[2] = (unsigned char) ((val >> 8) & 0x000000ff);
			delta[1] = (unsigned char) ((val >> 16) & 0x000000ff);
			delta[0] = (unsigned char) ((val >> 24) & 0x000000ff);
			status = PcdValue(PICC_INCREMENT, blk_nr, delta);//扣款
			
			if (status != MI_OK)
			{
				printf("blk_nr: %d add val failed\n", blk_nr);
				return status;

			}
			
			break;

		case CARD_CMD_DEC_VAL:
			printf("enter blk nr to dec\n");
			scanf("%d", &blk_nr);
			printf("enter val to dec\n");
			scanf("%ul", &val);
			delta[3] = (unsigned char) (val & 0x000000ff);
			delta[2] = (unsigned char) ((val >> 8) & 0x000000ff);
			delta[1] = (unsigned char) ((val >> 16) & 0x000000ff);
			delta[0] = (unsigned char) ((val >> 24) & 0x000000ff);
			status = PcdValue(PICC_DECREMENT, blk_nr, delta);//充值
			
			if (status != MI_OK)
			{
				printf("blk_nr: %d dec val failed\n", blk_nr);
				return status;

			}
		
			break;
		default:
			printf("do not support cmd %d \n", cmd);
			break;

	}

	return ret;

}

int main(int argc, char *argv[])
{
    int ret = 0;
	unsigned char status,i;
	int cmd;

	if(argc < 2)
	{
		printf("usage: rc522_spidev [dev-path] \n");
		return -1;
	}

	fd_spidev = open(argv[1], O_RDWR);
	
	if (fd_spidev < 0)
	{
		printf("can't open device\n");
		return -1;
	}

	/*
	 * spi mode
	 */
	ret = ioctl(fd_spidev, SPI_IOC_WR_MODE32, &mode);
	
	if (ret == -1)
	{
		printf("can't set spi mode\n");
		return ret;
	}

	ret = ioctl(fd_spidev, SPI_IOC_RD_MODE32, &mode);
	
	if (ret == -1)
	{
		printf("can't get spi mode\n");
		return ret;
	}

	/*
	 * bits per word
	 */
	ret = ioctl(fd_spidev, SPI_IOC_WR_BITS_PER_WORD, &bits);
	
	if (ret == -1)
	{
		printf("can't set bits per word\n");
		return ret;
	}

	ret = ioctl(fd_spidev, SPI_IOC_RD_BITS_PER_WORD, &bits);
	
	if (ret == -1)
	{
		printf("can't get bits per word\n");
		return ret;
	}

	/*
	 * max speed hz
	 */
	ret = ioctl(fd_spidev, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
	
	if (ret == -1)
	{
		printf("can't set max speed hz\n");
		return ret;
	}
		

	ret = ioctl(fd_spidev, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
	
	if (ret == -1)
	{
		printf("can't get max speed hz\n");
		return ret;
	}

	PcdReset();
	PcdAntennaOff();
 	PcdAntennaOn();

	while (1)
    {   
		status = pre_card_opr_flow();

		if (status != MI_OK)
			continue;

		printf("--------------new card opr cycle---------------\n");
		
		for(i = 0; NULL != card_ops[i]; i++)
		{
			printf("enter: %d to do %s \n", i, card_ops[i]);

		}

		scanf("%d", &cmd);
		do_card_opr(cmd);
		PcdHalt();
		printf("-----------------------------------------------\n");

    }

	close(fd_spidev);
	
    return 0;
}

