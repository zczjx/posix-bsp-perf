#include "bsp_v4l2_cap.h"
#include "ilclient.h"
#include <bcm_host.h>

#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>

#include <gmodule.h>
#include <glib.h>

#define X_RESOLUTION 1280
#define Y_RESOLUTION 720
#define V4L2_BUF_NR (4)
#define WATER_MASK (8)
#define DEFAULT_FRAMES (600)

typedef struct rpi_image {
	__u32 bytes;
	char *addr;
} rpi_image;


typedef struct omx_h264_ctx {
	ILCLIENT_T *client;
	COMPONENT_T *venc;
	OMX_BUFFERHEADERTYPE *in_buf;
	OMX_BUFFERHEADERTYPE *out_buf;
	int in_port;
    int out_port;
} omx_h264_ctx;


static int xres, yres;
static int v4l2_run = 0;
// GMutex que_lock;

static int init_omx_h264_enc(struct omx_h264_ctx *enc_ctx);

static int deinit_omx_h264_enc(struct omx_h264_ctx *enc_ctx);

static int omx_h264_encode(struct omx_h264_ctx *enc_ctx, 
	struct rpi_image *pix_dat);


static gpointer *v4l2_cap_task(gpointer data);

int main(int argc, char **argv)
{
	struct rpi_image *tmp_img = NULL;
	int i, err, ret = 0;
	long pre_time = 0;
	long curr_time = 0;
	int fps = 0;
	GThread *v4l2_task = NULL;
	char *out_fmt = NULL;
	GAsyncQueue *image_que = NULL;
	struct omx_h264_ctx enc_ctx;
	int framenumber = 0;
	FILE *f_out;
	char *filename = NULL;

	if(argc < 4)
	{
		printf("usage: rpi_mjpeg_v4l2_omx_dec [xres] [yres] [filename]\n");
		return -1;
	}

	xres = atoi(argv[1]);
	yres = atoi(argv[2]);
	filename = argv[3];
	image_que = g_async_queue_new();
	g_async_queue_ref(image_que);
    // Setup of the dec requests
    memset(&enc_ctx, 0x00, sizeof(struct omx_h264_ctx));
	bcm_host_init();
	err = init_omx_h264_enc(&enc_ctx);

	if (err != 0) 
	{
		perror("init_omx_h264_enc \n");
		return -1;
    }

	f_out = fopen(filename, "w");

	if (f_out == NULL) 
	{
		printf("Failed to open '%s' for writing video\n", filename);
		exit(1);
   }
	
	v4l2_run = 1;
	v4l2_task = g_thread_new("v4l2_cap_task", v4l2_cap_task, image_que);

	while(framenumber < DEFAULT_FRAMES)
	{
	
		tmp_img = g_async_queue_pop(image_que);
		err = omx_h264_encode(&enc_ctx, tmp_img);
		framenumber++;
		g_free(tmp_img->addr);
		g_free(tmp_img);
		tmp_img = NULL;
		bsp_print_fps("omx h264 hw enc", &fps, &pre_time, &curr_time);

		ret = fwrite(enc_ctx.out_buf->pBuffer, 1, enc_ctx.out_buf->nFilledLen, f_out);
		
		if (ret != enc_ctx.out_buf->nFilledLen) 
		{
	       printf("fwrite: Error emptying buffer: %d!\n", ret);
	    }
	    else 
		{
	       printf("Writing frame %d/%d\n", framenumber, DEFAULT_FRAMES);
	    }

		enc_ctx.out_buf->nFilledLen = 0;
		
	}

	fclose(f_out);
	g_async_queue_unref(image_que);
	deinit_omx_h264_enc(&enc_ctx);

    return 0;
}

static gpointer *v4l2_cap_task(gpointer data)
{
	int vfd;
	struct v4l2_buffer vbuf_param;
	struct rpi_image *tmp_img = NULL;
	struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
	struct bsp_v4l2_param v4l2_param;
	GAsyncQueue *image_que = (GAsyncQueue *) data;

	printf("start v4l2_cap_task!\n");
	vfd = bsp_v4l2_open_dev("/dev/video0");
	v4l2_param.fps = 30;
	v4l2_param.pixelformat = V4L2_PIX_FMT_YUV420;
	v4l2_param.xres = xres;
	v4l2_param.yres = yres;
	bsp_v4l2_try_setup(vfd, &v4l2_param);
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);
	bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR);
	bsp_v4l2_stream_on(vfd);
	g_async_queue_ref(image_que);

	while(v4l2_run)
	{
		bsp_v4l2_get_frame(vfd, &vbuf_param);

		if(g_async_queue_length(image_que) < WATER_MASK)
		{
			tmp_img = g_new(struct rpi_image, 1);
			tmp_img->bytes = v4l2_buf[vbuf_param.index].bytes;
			tmp_img->addr = g_malloc(tmp_img->bytes);
			memcpy(tmp_img->addr, v4l2_buf[vbuf_param.index].addr, v4l2_buf[vbuf_param.index].bytes);
			g_async_queue_push(image_que, tmp_img);
		}

		bsp_v4l2_put_frame_buf(vfd, &vbuf_param);
	}
	
	g_async_queue_unref(image_que);
	g_thread_yield ();

}

static void print_def(OMX_PARAM_PORTDEFINITIONTYPE def)
{
   printf("Port %u: %s %u/%u %u %u %s,%s,%s %ux%u %ux%u @%u %u\n",
	  def.nPortIndex,
	  def.eDir == OMX_DirInput ? "in" : "out",
	  def.nBufferCountActual,
	  def.nBufferCountMin,
	  def.nBufferSize,
	  def.nBufferAlignment,
	  def.bEnabled ? "enabled" : "disabled",
	  def.bPopulated ? "populated" : "not pop.",
	  def.bBuffersContiguous ? "contig." : "not cont.",
	  def.format.video.nFrameWidth,
	  def.format.video.nFrameHeight,
	  def.format.video.nStride,
	  def.format.video.nSliceHeight,
	  def.format.video.xFramerate, def.format.video.eColorFormat);
}


static int init_omx_h264_enc(struct omx_h264_ctx *enc_ctx)
{
	int err = 0;
	OMX_PARAM_PORTDEFINITIONTYPE def;
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	OMX_PORT_PARAM_TYPE port;
	OMX_VIDEO_PARAM_BITRATETYPE bitrateType;

	if(NULL == enc_ctx)
	{
		printf("%s:%d: input NULL enc_ctx! \n", __FUNCTION__, __LINE__);
		return -1;
	}

	enc_ctx->client = ilclient_init();

	if(NULL == enc_ctx->client)
	{
		printf("%s:%d: ilclient_init failed! \n", __FUNCTION__, __LINE__);
		return -3;
	}

	err = OMX_Init();
	
	if (err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_Init failed! \n", __FUNCTION__, __LINE__);
		ilclient_destroy(enc_ctx->client);
		return -4;
	}
	
	// create video_encode
	err = ilclient_create_component(enc_ctx->client, &enc_ctx->venc, "video_encode",
									ILCLIENT_DISABLE_ALL_PORTS |
				 					ILCLIENT_ENABLE_INPUT_BUFFERS |
				 					ILCLIENT_ENABLE_OUTPUT_BUFFERS);
	
	if (err != 0) 
	{
		printf("%s:%d: ilclient_create_component failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;
    OMX_GetParameter(ILC_GET_HANDLE(enc_ctx->venc), OMX_IndexParamVideoInit, &port);
	
    if (port.nPorts != 2) 
	{
		printf("%s:%d: port.nPorts == %d \n", __FUNCTION__, __LINE__, port.nPorts);
		return -1;
    }

	enc_ctx->in_port = port.nStartPortNumber;
	enc_ctx->out_port = port.nStartPortNumber + 1;
	
	// setup input port
	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = enc_ctx->out_port;
	err = OMX_GetParameter(ILC_GET_HANDLE(enc_ctx->venc), 
						OMX_IndexParamPortDefinition, &def); 
	
	if(err != OMX_ErrorNone) 
	{
		 printf("%s:%d: OMX_GetParameter() for video_encode port: %d failed!\n",
			__FUNCTION__, __LINE__, def.nPortIndex);
		 exit(1);
	}
	
	print_def(def);
	
	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = enc_ctx->in_port;
	err = OMX_GetParameter(ILC_GET_HANDLE(enc_ctx->venc), 
						OMX_IndexParamPortDefinition, &def); 
	
	if(err != OMX_ErrorNone) 
	{
		 printf("%s:%d: OMX_GetParameter() for video_encode port: %d failed!\n",
			__FUNCTION__, __LINE__, def.nPortIndex);
		 exit(1);
	}
	
	print_def(def);
	def.format.video.nFrameWidth = xres;
	def.format.video.nFrameHeight = yres;
	def.format.video.xFramerate = 30;
	def.format.video.nSliceHeight = def.format.video.nFrameHeight;
	def.format.video.nStride = def.format.video.nFrameWidth;
	def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
	err = OMX_SetParameter(ILC_GET_HANDLE(enc_ctx->venc),
						OMX_IndexParamPortDefinition, &def);
	
	if (err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_SetParameter() for video_encode port: %d failed\n",
		 __FUNCTION__, __LINE__, def.nPortIndex);
		exit(1);
	}
	
	err = OMX_GetParameter(ILC_GET_HANDLE(enc_ctx->venc), 
							OMX_IndexParamPortDefinition, &def);
		
	if(err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_GetParameter() for video_encode port: %d failed!\n",
			__FUNCTION__, __LINE__, def.nPortIndex);
		exit(1);
	}

	print_def(def);

	// setup output port
	
	
	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = enc_ctx->out_port;
	format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	err = OMX_SetParameter(ILC_GET_HANDLE(enc_ctx->venc), OMX_IndexParamVideoPortFormat, &format);

	if (err != 0) 
	{
		printf("%s:%d: OMX_SetParameter failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}
	
	// set current bitrate to 10Mbit
	memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
	bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
	bitrateType.nVersion.nVersion = OMX_VERSION;
	bitrateType.eControlRate = OMX_Video_ControlRateVariable;
	bitrateType.nTargetBitrate = 10000000;
	bitrateType.nPortIndex = enc_ctx->out_port;
	err = OMX_SetParameter(ILC_GET_HANDLE(enc_ctx->venc),
					OMX_IndexParamVideoBitrate, &bitrateType);
	
	if (err != OMX_ErrorNone) 
	{
	   printf
		 ("%s:%d: OMX_SetParameter() for bitrate for video_encode port %d failed!\n",
			__FUNCTION__, __LINE__, bitrateType.nPortIndex);
	   exit(1);
	}
	
	// get current bitrate
	memset(&bitrateType, 0, sizeof(OMX_VIDEO_PARAM_BITRATETYPE));
	bitrateType.nSize = sizeof(OMX_VIDEO_PARAM_BITRATETYPE);
	bitrateType.nVersion.nVersion = OMX_VERSION;
	bitrateType.nPortIndex = enc_ctx->out_port;
	err = OMX_GetParameter(ILC_GET_HANDLE(enc_ctx->venc), OMX_IndexParamVideoBitrate,
						&bitrateType);
	
	if (err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_GetParameter() for video_encode for bitrate port %d failed!\n",
				__FUNCTION__, __LINE__, bitrateType.nPortIndex);
	   exit(1);
	}
	
	printf("Current Bitrate=%u\n",bitrateType.nTargetBitrate);
	
	err = ilclient_change_component_state(enc_ctx->venc, OMX_StateIdle);

	if(err != 0)
	{
		printf("%s:%d: ilclient_change_component_state(video_encode, OMX_StateIdle) failed",
				__FUNCTION__, __LINE__);
	}
	
	err = ilclient_enable_port_buffers(enc_ctx->venc, enc_ctx->in_port, NULL, NULL, NULL);

	if (err != 0) 
	{
		printf("%s:%d: ilclient_enable_port_buffers failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	err = ilclient_enable_port_buffers(enc_ctx->venc, enc_ctx->out_port, NULL, NULL, NULL);

	if (err != 0) 
	{
		printf("%s:%d: ilclient_enable_port_buffers failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	enc_ctx->in_buf = NULL;
	enc_ctx->out_buf = NULL;
	
	err = ilclient_change_component_state(enc_ctx->venc, OMX_StateExecuting);

	if(err != 0)
	{
		printf("%s:%d: ilclient_change_component_state(video_encode, OMX_StateExecuting) failed",
				__FUNCTION__, __LINE__);
	}

    return 0;
}

static int deinit_omx_h264_enc(struct omx_h264_ctx *enc_ctx)
{
	int err = 0;
	COMPONENT_T *list[2];

	memset(list, 0, sizeof(list));
	list[0] = enc_ctx->venc;
	ilclient_disable_port_buffers(enc_ctx->venc, enc_ctx->in_port, NULL, NULL, NULL);
	ilclient_disable_port_buffers(enc_ctx->venc, enc_ctx->out_port, NULL, NULL, NULL);
	ilclient_change_component_state(enc_ctx->venc, OMX_StateIdle);
	ilclient_change_component_state(enc_ctx->venc, OMX_StateLoaded);
	ilclient_cleanup_components(list);
   	OMX_Deinit();
	ilclient_destroy(enc_ctx->client);

	return 0;

}


static int omx_h264_encode(struct omx_h264_ctx *enc_ctx, 
	struct rpi_image *pix_dat)
{
	int err = 0;

	enc_ctx->in_buf = ilclient_get_input_buffer(enc_ctx->venc, enc_ctx->in_port, 1);

	if (enc_ctx->in_buf == NULL) 
	{
		printf("%s:%d: ilclient_get_input_buffer failed \n", __FUNCTION__, __LINE__);
		return -1;
	}

	if(pix_dat->bytes <= enc_ctx->in_buf->nAllocLen)
	{
		memcpy(enc_ctx->in_buf->pBuffer, pix_dat->addr, pix_dat->bytes);
		enc_ctx->in_buf->nFilledLen = pix_dat->bytes;
	}
	else
	{
		printf("%s:%d: data len is larger than buf alloc len!\n", __FUNCTION__, __LINE__);
		memcpy(enc_ctx->in_buf->pBuffer, pix_dat->addr, enc_ctx->in_buf->nAllocLen);
		enc_ctx->in_buf->nFilledLen = enc_ctx->in_buf->nAllocLen;
	}

	err = OMX_EmptyThisBuffer(ILC_GET_HANDLE(enc_ctx->venc), enc_ctx->in_buf);

	if (err != OMX_ErrorNone)
	{
	    printf("%s:%d: OMX_EmptyThisBuffer failed!\n", __FUNCTION__, __LINE__);
	}
	
	enc_ctx->out_buf = ilclient_get_output_buffer(enc_ctx->venc, enc_ctx->out_port, 1);

	if (enc_ctx->out_buf == NULL) 
	{
		printf("%s:%d: ilclient_get_output_buffer failed \n", __FUNCTION__, __LINE__);
		return -1;
	}

	err = OMX_FillThisBuffer(ILC_GET_HANDLE(enc_ctx->venc), enc_ctx->out_buf);
	
	if (err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_FillThisBuffer failed!\n", __FUNCTION__, __LINE__);
	}

    return 0;

}


