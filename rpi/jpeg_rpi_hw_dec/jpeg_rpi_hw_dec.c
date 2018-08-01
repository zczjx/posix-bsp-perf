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

static uint8_t encodedInBuf[MAX_ENCODED];
static uint8_t encodedOutBuf[MAX_ENCODED];
static uint8_t decodedBuf[MAX_DECODED];
static char outFileName[2048];

#define V4L2_BUF_NR (4)
struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
struct bsp_v4l2_param v4l2_param;

int main(int argc, char **argv)
{
    BRCMJPEG_STATUS_T status;
    BRCMJPEG_REQUEST_T dec_request;
    BRCMJPEG_T *dec = 0;
    unsigned int count = 1, format = PIXEL_FORMAT_YUYV;
    unsigned int handle = 0, vc_handle = 0;
    int i, arg = 1, help = 0;
	int err, vfd, pts = 0;
	struct timespec tp;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	int buf_idx = 0;

    // Setup of the dec requests
    memset(&dec_request, 0, sizeof(dec_request));
    dec_request.input = encodedInBuf;
    dec_request.output = decodedBuf;
    dec_request.output_handle = 0;
    dec_request.output_alloc_size = MAX_DECODED;

    status = brcmjpeg_create(BRCMJPEG_TYPE_DECODER, &dec);
    if (status != BRCMJPEG_SUCCESS)
    {
        fprintf(stderr, "could not create decoder\n");
        return -1;
    }

	vfd = bsp_v4l2_open_dev("/dev/video0");
	v4l2_param.fps = 30;
	v4l2_param.pixelformat = V4L2_PIX_FMT_MJPEG;
	v4l2_param.xres = 1280;
	v4l2_param.yres = 720;
	bsp_v4l2_try_setup(vfd, &v4l2_param);
	bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR);
	bsp_v4l2_stream_on(vfd);
	
	while(++pts <= 50000)
	{
		bsp_print_fps("mjpeg hw dec", &fps, &pre_time, &curr_time);
		bsp_v4l2_get_frame(vfd, &buf_idx);
		memcpy(encodedInBuf, v4l2_buf[buf_idx].addr, v4l2_buf[buf_idx].bytes);
		dec_request.input_size = v4l2_buf[buf_idx].bytes;
		dec_request.buffer_width = 1280;
		dec_request.buffer_height = 720;
		bsp_v4l2_put_frame_buf(vfd, buf_idx);
		status = brcmjpeg_process(dec, &dec_request);

		if (status != BRCMJPEG_SUCCESS) 
		{
			fprintf(stderr, "could not decode \n");
			break;
		}
		
		printf("dec_request.output_size: %d \n", dec_request.output_size);
    
	}

    brcmjpeg_release(dec);

    return 0;
}


