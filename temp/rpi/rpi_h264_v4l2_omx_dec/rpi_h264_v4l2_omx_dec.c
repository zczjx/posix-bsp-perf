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
#define DEFAULT_DEC_FRAMES (600)
#define ENCODE_FRAME_TYPE_MASK (V4L2_BUF_FLAG_KEYFRAME | V4L2_BUF_FLAG_PFRAME | \
								V4L2_BUF_FLAG_BFRAME)
								


typedef struct rpi_image {
	__u32 bytes;
	char *addr;
	__u32 encode_frame_type;
} rpi_image;

typedef struct omx_com_dsc {
	COMPONENT_T *component;
	int in_port;
	int out_port;
} omx_com_dsc;

typedef struct omx_h264_ctx {
	ILCLIENT_T *client;
	TUNNEL_T tunnel[4];
	COMPONENT_T *list[5];
	int list_cnt;
	struct omx_com_dsc vdec;
	struct omx_com_dsc vrender;
	OMX_BUFFERHEADERTYPE *in_buf;
	OMX_BUFFERHEADERTYPE *out_buf;
} omx_h264_ctx;

static int xres, yres;
static int v4l2_run = 0;

// GMutex que_lock;

static int init_omx_h264_dec(struct omx_h264_ctx *dec_ctx);

static int deinit_omx_h264_dec(struct omx_h264_ctx *dec_ctx);

static int omx_h264_decode(struct omx_h264_ctx *dec_ctx, 
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
	struct omx_h264_ctx dec_ctx;
	int framenumber = 0;

	if(argc < 3)
	{
		printf("usage: rpi_mjpeg_v4l2_omx_dec [xres] [yres]\n");
		return -1;
	}

	xres = atoi(argv[1]);
	yres = atoi(argv[2]);
	image_que = g_async_queue_new();
	g_async_queue_ref(image_que);
    // Setup of the dec requests
    memset(&dec_ctx, 0x00, sizeof(struct omx_h264_ctx));
	bcm_host_init();
	err = init_omx_h264_dec(&dec_ctx);

	if (err != 0) 
	{
		perror("init_omx_h264_dec \n");
		return -1;
    }
	
	v4l2_run = 1;
	v4l2_task = g_thread_new("v4l2_cap_task", v4l2_cap_task, image_que);

	while(framenumber < DEFAULT_DEC_FRAMES)
	{
		tmp_img = g_async_queue_pop(image_que);
		err = omx_h264_decode(&dec_ctx, tmp_img);
		framenumber++;
		g_free(tmp_img->addr);
		g_free(tmp_img);
		tmp_img = NULL;
		bsp_print_fps("omx h264 hw dec", &fps, &pre_time, &curr_time);		
	}

	dec_ctx.in_buf->nFilledLen = 0;
	dec_ctx.in_buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;
	OMX_EmptyThisBuffer(ILC_GET_HANDLE(dec_ctx.vdec.component), 
							dec_ctx.in_buf);
	// need to flush the renderer to allow video_decode to disable its input port
	ilclient_flush_tunnels(dec_ctx.tunnel, 0);

	printf("finish dec %d frames\n", framenumber);
	g_async_queue_unref(image_que);
	deinit_omx_h264_dec(&dec_ctx);

    return 0;
}

static gpointer *v4l2_cap_task(gpointer data)
{
	int vfd;
	struct v4l2_buffer vbuf_param;
	struct rpi_image *tmp_img = NULL;
	struct bsp_v4l2_cap_buf v4l2_buf[V4L2_BUF_NR];
	struct bsp_v4l2_param v4l2_param;
	int mp_buf_flag;
	GAsyncQueue *image_que = (GAsyncQueue *) data;

	printf("start v4l2_cap_task!\n");
	vfd = bsp_v4l2_open_dev("/dev/video0", &mp_buf_flag);
	v4l2_param.fps = 30;
	v4l2_param.pixelformat = V4L2_PIX_FMT_H264;
	v4l2_param.xres = xres;
	v4l2_param.yres = yres;
	bsp_v4l2_try_setup(vfd, &v4l2_param, mp_buf_flag);
	printf("v4l2_param.fps: %d \n", v4l2_param.fps);
	printf("v4l2_param.pixelformat: 0x%x \n", v4l2_param.pixelformat);
	printf("v4l2_param.xres: %d \n", v4l2_param.xres);
	printf("v4l2_param.yres: %d \n", v4l2_param.yres);
	bsp_v4l2_req_buf(vfd, v4l2_buf, V4L2_BUF_NR, mp_buf_flag);
	bsp_v4l2_stream_on(vfd, mp_buf_flag);
	g_async_queue_ref(image_que);

	while(v4l2_run)
	{
		bsp_v4l2_get_frame(vfd, &vbuf_param, mp_buf_flag);

		if(g_async_queue_length(image_que) < WATER_MASK)
		{
			tmp_img = g_new(struct rpi_image, 1);
			tmp_img->bytes = v4l2_buf[vbuf_param.index].bytes;
			tmp_img->addr = g_malloc(tmp_img->bytes);
			memcpy(tmp_img->addr, v4l2_buf[vbuf_param.index].addr, v4l2_buf[vbuf_param.index].bytes);
			tmp_img->encode_frame_type = vbuf_param.flags & ENCODE_FRAME_TYPE_MASK;
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

static int init_omx_h264_dec(struct omx_h264_ctx *dec_ctx)
{
	int err = 0;
	OMX_PARAM_PORTDEFINITIONTYPE def;
	OMX_VIDEO_PARAM_PORTFORMATTYPE format;
	OMX_PORT_PARAM_TYPE port;

	if(NULL == dec_ctx)
	{
		printf("%s:%d: input NULL dec_ctx! \n", __FUNCTION__, __LINE__);
		return -1;
	}

	memset(dec_ctx->list, 0, sizeof(dec_ctx->list));
	dec_ctx->list_cnt = 0;
	memset(dec_ctx->tunnel, 0, sizeof(dec_ctx->tunnel));
	dec_ctx->client = ilclient_init();

	if(NULL == dec_ctx->client)
	{
		printf("%s:%d: ilclient_init failed! \n", __FUNCTION__, __LINE__);
		return -3;
	}

	err = OMX_Init();
	
	if (err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_Init failed! \n", __FUNCTION__, __LINE__);
		ilclient_destroy(dec_ctx->client);
		return -4;
	}
	
	// create video_decode
	err = ilclient_create_component(dec_ctx->client, &dec_ctx->vdec.component, 
						"video_decode", 
						ILCLIENT_DISABLE_ALL_PORTS 
						| ILCLIENT_ENABLE_INPUT_BUFFERS);
	
	if (err != 0) 
	{
		printf("%s:%d: ilclient_create_component failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	dec_ctx->list[dec_ctx->list_cnt] = dec_ctx->vdec.component;
	dec_ctx->list_cnt++;

	// create video_render
	err = ilclient_create_component(dec_ctx->client, &dec_ctx->vrender.component, 
						"video_render", ILCLIENT_DISABLE_ALL_PORTS);
	
	if (err != 0) 
	{
		printf("%s:%d: ilclient_create_component failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	dec_ctx->list[dec_ctx->list_cnt] = dec_ctx->vrender.component;
	dec_ctx->list_cnt++;

	// setup video_decode and input port
	memset(&port, 0, sizeof(OMX_PORT_PARAM_TYPE));
	port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;
    OMX_GetParameter(ILC_GET_HANDLE(dec_ctx->vdec.component), OMX_IndexParamVideoInit, &port);
	
    if (port.nPorts != 2) 
	{
		printf("%s:%d: port.nPorts == %d \n", __FUNCTION__, __LINE__, port.nPorts);
		return -1;
    }

	dec_ctx->vdec.in_port = port.nStartPortNumber;
	dec_ctx->vdec.out_port = port.nStartPortNumber + 1;
	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = dec_ctx->vdec.in_port;
	format.eCompressionFormat = OMX_VIDEO_CodingAVC;
	err = OMX_SetParameter(ILC_GET_HANDLE(dec_ctx->vdec.component), 
						OMX_IndexParamVideoPortFormat, &format);

	if (err != 0) 
	{
		printf("%s:%d: OMX_SetParameter failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = dec_ctx->vdec.in_port;
	err = OMX_GetParameter(ILC_GET_HANDLE(dec_ctx->vdec.component), 
						OMX_IndexParamPortDefinition, &def); 
	
	if(err != OMX_ErrorNone) 
	{
		 printf("%s:%d: OMX_GetParameter() failed!\n",
			__FUNCTION__, __LINE__);
		 exit(1);
	} 

	printf("dec_ctx->vdec.in_port: \n");
	print_def(def);

	def.format.video.nFrameWidth = xres;
	def.format.video.nFrameHeight = yres;
	def.format.video.xFramerate = 30;
	def.format.video.nSliceHeight = yres;
	def.format.video.nStride = xres;
	def.format.video.eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
	def.nBufferSize = xres * yres * 2;
	err = OMX_SetParameter(ILC_GET_HANDLE(dec_ctx->vdec.component),
						OMX_IndexParamPortDefinition, &def);
	
	if (err != OMX_ErrorNone) 
	{
		printf("%s:%d: OMX_SetParameter() dec_ctx->vdec.in_port: %d failed\n",
		 __FUNCTION__, __LINE__, def.nPortIndex);
		exit(1);
	}
	
	err = OMX_GetParameter(ILC_GET_HANDLE(dec_ctx->vdec.component), 
						OMX_IndexParamPortDefinition, &def);
	printf("dec_ctx->vdec.in_port: \n");
	print_def(def);	
	
	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = dec_ctx->vdec.out_port;
	err = OMX_GetParameter(ILC_GET_HANDLE(dec_ctx->vdec.component), 
						OMX_IndexParamPortDefinition, &def); 
	
	if(err != OMX_ErrorNone) 
	{
		 printf("%s:%d: OMX_GetParameter() failed!\n",
			__FUNCTION__, __LINE__);
		 exit(1);
	} 

	printf("dec_ctx->vdec.out_port: \n");
	print_def(def);

	// setup video_render and port
	memset(&port, 0, sizeof(OMX_PORT_PARAM_TYPE));
	port.nSize = sizeof(OMX_PORT_PARAM_TYPE);
    port.nVersion.nVersion = OMX_VERSION;
    OMX_GetParameter(ILC_GET_HANDLE(dec_ctx->vrender.component), OMX_IndexParamVideoInit, &port);
	dec_ctx->vrender.in_port = port.nStartPortNumber;
	printf("dec_ctx->vrender.in_port: %d\n", dec_ctx->vrender.in_port);	

	memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
	format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
	format.nVersion.nVersion = OMX_VERSION;
	format.nPortIndex = dec_ctx->vrender.in_port;
	format.eColorFormat = OMX_COLOR_Format24bitBGR888;
	err = OMX_SetParameter(ILC_GET_HANDLE(dec_ctx->vrender.component), 
						OMX_IndexParamVideoPortFormat, &format);

	if (err != 0) 
	{
		printf("%s:%d: OMX_SetParameter failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}


	memset(&def, 0, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
	def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
	def.nVersion.nVersion = OMX_VERSION;
	def.nPortIndex = dec_ctx->vrender.in_port;
	err = OMX_GetParameter(ILC_GET_HANDLE(dec_ctx->vrender.component), 
						OMX_IndexParamPortDefinition, &def); 
	
	if(err != OMX_ErrorNone) 
	{
		 printf("%s:%d: OMX_GetParameter() failed!\n",
			__FUNCTION__, __LINE__);
		 exit(1);
	} 

	printf("dec_ctx->vrender.in_port: \n");
	print_def(def);

	//setup tunnel
	set_tunnel(&dec_ctx->tunnel[0], dec_ctx->vdec.component, dec_ctx->vdec.out_port, 
				dec_ctx->vrender.component, dec_ctx->vrender.in_port);
	err = ilclient_setup_tunnel(&dec_ctx->tunnel[0], 0, 0);
	
	if(err != OMX_ErrorNone) 
	{
		printf("%s:%d: ilclient_setup_tunnel()failed!\n",
			__FUNCTION__, __LINE__);
		exit(1);
	}
	
	err = ilclient_change_component_state(dec_ctx->vrender.component, OMX_StateIdle);

	if(err != 0)
	{
		printf("%s:%d: ilclient_change_component_state() failed",
				__FUNCTION__, __LINE__);
	}

	err = ilclient_change_component_state(dec_ctx->vrender.component, OMX_StateExecuting);

	if(err != 0)
	{
		printf("%s:%d: ilclient_change_component_state(video_encode, OMX_StateExecuting) failed",
				__FUNCTION__, __LINE__);
	}

	
	err = ilclient_change_component_state(dec_ctx->vdec.component, OMX_StateIdle);

	if(err != 0)
	{
		printf("%s:%d: ilclient_change_component_state() failed",
				__FUNCTION__, __LINE__);
	}

	err = ilclient_enable_port_buffers(dec_ctx->vdec.component, 
						dec_ctx->vdec.in_port, NULL, NULL, NULL);

	if (err != 0) 
	{
		printf("%s:%d: ilclient_enable_port_buffers failed \n", __FUNCTION__, __LINE__);
		exit(1);
	}

	err = ilclient_change_component_state(dec_ctx->vdec.component, OMX_StateExecuting);

	if(err != 0)
	{
		printf("%s:%d: ilclient_change_component_state() failed",
				__FUNCTION__, __LINE__);
	}

	dec_ctx->in_buf = NULL;
	dec_ctx->out_buf = NULL;

    return 0;
}

static int deinit_omx_h264_dec(struct omx_h264_ctx *dec_ctx)
{
	int err = 0;
	
	ilclient_disable_port_buffers(dec_ctx->vdec.component, 
								dec_ctx->vdec.in_port, NULL, NULL, NULL);
	ilclient_teardown_tunnels(dec_ctx->tunnel);
	ilclient_state_transition(dec_ctx->list, OMX_StateIdle);
	ilclient_state_transition(dec_ctx->list, OMX_StateLoaded);
	ilclient_cleanup_components(dec_ctx->list);
	dec_ctx->list_cnt = 0;
   	OMX_Deinit();
	ilclient_destroy(dec_ctx->client);

	return 0;

}


static int omx_h264_decode(struct omx_h264_ctx *dec_ctx, 
	struct rpi_image *pix_dat)
{
	int err = 0;

	dec_ctx->in_buf = ilclient_get_input_buffer(dec_ctx->vdec.component, 
											dec_ctx->vdec.in_port, 1);

	if (dec_ctx->in_buf == NULL) 
	{
		printf("%s:%d: ilclient_get_input_buffer failed \n", __FUNCTION__, __LINE__);
		return -1;
	}

	if(pix_dat->bytes <= dec_ctx->in_buf->nAllocLen)
	{
		memcpy(dec_ctx->in_buf->pBuffer, pix_dat->addr, pix_dat->bytes);
		dec_ctx->in_buf->nFilledLen = pix_dat->bytes;
	}
	else
	{
		printf("%s:%d: data len is larger than buf alloc len!\n", __FUNCTION__, __LINE__);
		memcpy(dec_ctx->in_buf->pBuffer, pix_dat->addr, dec_ctx->in_buf->nAllocLen);
		dec_ctx->in_buf->nFilledLen = dec_ctx->in_buf->nAllocLen;
	}

	err = OMX_EmptyThisBuffer(ILC_GET_HANDLE(dec_ctx->vdec.component), 
							dec_ctx->in_buf);

	if (err != OMX_ErrorNone)
	{
	    printf("%s:%d: OMX_EmptyThisBuffer failed!\n", __FUNCTION__, __LINE__);
	}

    return 0;

}



