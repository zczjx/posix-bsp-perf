/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "brcmjpeg.h"
#include "bsp_v4l2_cap.h"
#include "bsp_fb.h"

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_WIDTH   5000
#define MAX_HEIGHT  5000
#define MAX_ENCODED (15*1024*1024)
#define MAX_DECODED (MAX_WIDTH*MAX_HEIGHT*2)
#define X_RESOLUTION 480
#define Y_RESOLUTION 720


static uint8_t encodedInBuf[MAX_ENCODED];
static uint8_t encodedOutBuf[MAX_ENCODED];
static uint8_t decodedBuf[MAX_DECODED];
static char outFileName[2048];

#define V4L2_BUF_NR (4)
struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
struct bsp_v4l2_param v4l2_param;

static struct bsp_fb_var_attr fb_var_attr;
static struct bsp_fb_fix_attr fb_fix_attr;
static struct rgb_frame dsip_frame = {
	.addr = NULL,
};


int main(int argc, char **argv)
{
    BRCMJPEG_STATUS_T status;
    BRCMJPEG_REQUEST_T dec_request;
    BRCMJPEG_T *dec = 0;
    int i, xres, yres;
	int err, vfd, pts = 0;
	struct timespec tp;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	int buf_idx = 0;
	int disp_fd = 0;
	struct v4l2_buffer vbuf_param;
	

	xres = atoi(argv[1]);
	yres = atoi(argv[2]);

	disp_fd = bsp_fb_open_dev("/dev/fb0", &fb_var_attr, &fb_fix_attr);
	fb_var_attr.red_offset = 0;
	fb_var_attr.green_offset = 8;
	fb_var_attr.blue_offset = 16;
	fb_var_attr.transp_offset = 24;
	err = bsp_fb_try_setup(disp_fd, &fb_var_attr);
	
	if(err < 0)
	{
		fprintf(stderr, "could not bsp_fb_try_setup\n");
	}

    // Setup of the dec requests
    memset(&dec_request, 0, sizeof(dec_request));
    dec_request.input = encodedInBuf;
    dec_request.output = decodedBuf;
    dec_request.output_handle = 0;
    dec_request.output_alloc_size = MAX_DECODED;
	dec_request.buffer_width = xres;
	dec_request.buffer_height = yres;
	dec_request.pixel_format = PIXEL_FORMAT_RGBA;
    status = brcmjpeg_create(BRCMJPEG_TYPE_DECODER, &dec);
	
    if (status != BRCMJPEG_SUCCESS)
    {
        fprintf(stderr, "could not create decoder\n");
        return -1;
    }

	vfd = bsp_v4l2_open_dev("/dev/video0");
	v4l2_param.fps = 30;
	v4l2_param.pixelformat = V4L2_PIX_FMT_MJPEG;
	v4l2_param.xres = xres;
	v4l2_param.yres = yres;
	bsp_v4l2_try_setup(vfd, &v4l2_param);
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);
	bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR);
	bsp_v4l2_stream_on(vfd);

	while(++pts <= 50000)
	{
		bsp_print_fps("mjpeg hw dec", &fps, &pre_time, &curr_time);
		bsp_v4l2_get_frame(vfd, &vbuf_param);
		memcpy(encodedInBuf, v4l2_buf[vbuf_param.index].addr, v4l2_buf[vbuf_param.index].bytes);
		bsp_v4l2_put_frame_buf(vfd, &vbuf_param);
		dec_request.input_size = v4l2_buf[buf_idx].bytes;

		status = brcmjpeg_process(dec, &dec_request);

		if (status != BRCMJPEG_SUCCESS) 
		{
			fprintf(stderr, "could not decode \n");
			break;
		}

		dsip_frame.xres = xres;
		dsip_frame.yres = yres;
		dsip_frame.bits_per_pixel = fb_var_attr.bits_per_pixel;
		dsip_frame.bytes_per_line = xres * (dsip_frame.bits_per_pixel >> 3);
		dsip_frame.bytes = dec_request.output_size;
		
		if(NULL == dsip_frame.addr)
		{
			dsip_frame.addr = malloc(dec_request.output_size);
		}
		
		memcpy(dsip_frame.addr, decodedBuf, dsip_frame.bytes);
		bsp_fb_flush(disp_fd, &fb_var_attr, &fb_fix_attr, &dsip_frame);

	}

    brcmjpeg_release(dec);

    return 0;
}


