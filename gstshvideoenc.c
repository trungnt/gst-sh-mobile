/**
 * \page enc gst-sh-mobile-enc
 * gst-sh-mobile-enc - Encodes raw YUV image data to MPEG4/H264 video stream on
 * SuperH environment using libshcodecs HW codec.
 *
 * \section dec-description Description
 * This element is designed to use the HW video processing modules of the Renesas
 * SuperH chipset to encode mpeg4/h264 video streams. This element is not usable
 * in any other environments and it requires libshcodes HW codec to be installed.
 * 
 * The encoding settings are given as properties to the encoder or using a control
 * file. Examples of control files can be found from /cntl_file -folder.
 * 
 * \section enc-examples Example launch lines
 * \subsection enc-examples-1 Encoding from a file to a file
 * \code
 * gst-launch filesrc location=test.yuv ! gst-sh-mobile-enc stream-type=mpeg4
 * width=320 height=240 framerate=300 ! filesink location=test.m4v
 * \endcode
 * This is a very simple pipeline where filesrc and filesink elements are used
 * to read the raw data and write the encoded data. In this pipeline the
 * gst-sh-mobile-enc operates in pull mode, so it is the element which drives the
 * data flow.
 *
 * \subsection enc-examples-2 Encoding from a webcam to a file
 * \code
 * gst-launch v4l2src device=/dev/video0 ! image/jpeg, width=320, height=240,
 * framerate=15/1 ! jpegdec ! ffmpegcolorspace ! gst-sh-mobile-enc
 * stream-type=mpeg4 bitrate=250000 ! filesink location=test.m4v
 * \endcode
 * 
 * \image html encoder_example.jpeg
 * 
 * In this example, webcam is used as the streaming source via v4l2src element.
 * the webcam in this example provides jpeg image data. This pipeline works in
 * push mode, so we need to specify the stream properties using static caps after
 * the v4l2src element. jpegdec decodes the jpeg images and ffmpegcolorspace is
 * used to convert the image data for the encoder. Again, filesink is used to
 * write the encoded video stream into a file.
 *
 * \subsection enc-examples-3 Encoding from a webcam to network
 * \code
 * gst-launch v4l2src device=/dev/video0 ! image/jpeg, width=320, height=240,
 * framerate=15/1 ! jpegdec ! ffmpegcolorspace ! gst-sh-mobile-enc
 * ! rtpmp4vpay ! udpsink host=192.168.10.10 port=5000 sync=false 
 * \endcode
 * This line is similar to the one above. At this time, the video is not stored
 * in a file but sent over the network using udpsink -element. Before sending,
 * the video is packed into RTP frame using rtpmp4vpay -element.
 *
 * The following line allows playback of the video in PC:
 * \code
 * gst-launch udpsrc port=5000 caps="application/x-rtp, clock-rate=90000"
 * ! gstrtpjitterbuffer latency=0 ! rtpmp4vdepay ! video/mpeg, width=320,
 * height=240, framerate=15/1 ! ffdec_mpeg4 ! ffmpegcolorspace ! ximagesink 
 * \endcode
 * 
 * \section enc-properties Properties
 * \copydoc gst_sh_video_enc_properties
 *
 * \section enc-pads Pads
 * \copydoc enc_sink_factory
 * \copydoc enc_src_factory
 * 
 * \section enc-license License
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
 *
 * Johannes Lahti <johannes.lahti@nomovok.com>
 * Pablo Virolainen <pablo.virolainen@nomovok.com>
 * Aki Honkasuo <aki.honkasuo@nomovok.com>
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>

#include <gst/gst.h>

#include "gstshvideoenc.h"
#include "gstshencdefaults.h"
#include "cntlfile/ControlFileUtil.h"

/**
 * \var enc_sink_factory
 * Name: sink \n
 * Direction: sink \n
 * Available: always \n
 * Caps:
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[48, 720], 
 *   height=(int)[48, 576], framerate=(fraction)[1, 25]
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[48, 720], 
 *   height=(int)[48, 480], framerate=(fraction)[1, 30]
 */
static GstStaticPadTemplate enc_sink_factory = 
	GST_STATIC_PAD_TEMPLATE("sink",
				 GST_PAD_SINK,
				 GST_PAD_ALWAYS,
				 GST_STATIC_CAPS(
						"video/x-raw-yuv, "
						"format = (fourcc) NV12,"
						"width = (int) [48, 720],"
						"height = (int) [48, 480]," 
						"framerate = (fraction) [0, 30]"
						";"
						"video/x-raw-yuv, "
						"format = (fourcc) NV12,"
						"width = (int) [48, 720],"
						"height = (int) [48, 576]," 
						"framerate = (fraction) [0, 25]"
						)
				 );


/**
 * \var enc_src_factory
 * Name: src \n
 * Direction: src \n
 * Available: always \n
 * Caps:
 * - video/mpeg, width=(int)[48, 720], height=(int)[48, 576], 
 *   framerate=(fraction)[1, 25], mpegversion=(int)4
 * - video/mpeg, width=(int)[48, 720], height=(int)[48, 480], 
 *   framerate=(fraction)[1, 30], mpegversion=(int)4
 * - video/x-h264, width=(int)[48, 720], height=(int)[48, 576], 
 *   framerate=(fraction)[1, 25], h264version=(int)h264
 * - video/x-h264, width=(int)[48, 720], height=(int)[48, 480], 
 *   framerate=(fraction)[1, 30], h264version=(int)h264
 */
static GstStaticPadTemplate enc_src_factory = 
	GST_STATIC_PAD_TEMPLATE("src",
				 GST_PAD_SRC,
				 GST_PAD_ALWAYS,
				 GST_STATIC_CAPS(
						"video/mpeg,"
						"width  = (int) [48, 720],"
						"height = (int) [48, 576],"
						"framerate = (fraction) [0, 25],"
						"mpegversion = (int) 4"
						";"
						"video/mpeg,"
						"width  = (int) [48, 720],"
						"height = (int) [48, 480],"
						"framerate = (fraction) [0, 30],"
						"mpegversion = (int) 4"
						"; "
						"video/x-h264,"
						"width  = (int) [48, 720],"
						"height = (int) [48, 576],"
						"framerate = (fraction) [0, 25],"
						"variant = (string) itu,"
						"h264version = (string) h264"
						"; "
						"video/x-h264,"
						"width  = (int) [48, 720],"
						"height = (int) [48, 480],"
						"framerate = (fraction) [0, 30],"
						"variant = (string) itu,"
						"h264version = (string) h264"
						)
				 );

GST_DEBUG_CATEGORY_STATIC(gst_sh_mobile_debug);
#define GST_CAT_DEFAULT gst_sh_mobile_debug

static GstElementClass *parent_class = NULL;

/**
 * \enum gst_sh_video_enc_properties
 * Here is the list of the most important properties of gst-sh-mobile-enc.
 * There are a lot of other available properties, see the source code for more
 * information.
 * - "cntl-file" (string). Name of the control file containing encoding
 *   parameters. Default: NULL
 * - "stream-type" (string). The type of the video stream ("h264"/"mpeg4").
 *   Default: None (An error message will display if the property is not set 
 *   or can not be determined from the stream). 
 * - "width" (long). The width of the video stream (48-720px). Default: 0 
 *   (An error message will display if the property is not set or can not 
 *   be determined from the stream). 
 * - "height" (long). The width of the video stream (48-576px). Default: 0 
 *   (An error message will display if the property is not set or can not 
 *   be determined from the stream). 
 * - "framerate" (long). The framerate of the video stream multiplied by 10 (0-300).
 *   Default: 0 (An error message will display if the property is not set or 
 *   can not be determined from the stream).
 * - "bitrate" (long). The bitrate of the video stream (0-10000000). 
 *   Default: 384000 for mpeg4 and 2000000 for h264
 * - "i-vop-interval" (long). Interval of intra-coded video object planes. 
 *   Default: 30
 * - "noise-reduction" (long). For motion-compensated macroblocks, a difference
 *    equal to or smaller than this setting is treated as 0 for encoding (0-4).
 *    Default: 0.
 * - "weighted-q-mode" (long). Used to specify whether weighted quantization for 
 *   encoding is used or not (0/1). Default: 0.
 */
enum gst_sh_video_enc_properties
{
	PROP_0,
	PROP_CNTL_FILE,
	/* COMMON */
	PROP_STREAM_TYPE,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_FRAMERATE,
	PROP_BITRATE,
	PROP_I_VOP_INTERVAL,
	PROP_MV_MODE,
	PROP_FCODE_FORWARD,
	PROP_SEARCH_MODE,
	PROP_SEARCH_TIME_FIXED,
	PROP_RATECONTROL_SKIP_ENABLE,
	PROP_RATECONTROL_USE_PREVQUANT,
	PROP_RATECONTROL_RESPECT_TYPE,
	PROP_RATECONTROL_INTRA_THR_CHANGEABLE,
	PROP_CONTROL_BITRATE_LENGTH,
	PROP_INTRA_MACROBLOCK_REFRESH_CYCLE,
	PROP_VIDEO_FORMAT,
	PROP_FRAME_NUM_RESOLUTION,
	PROP_NOISE_REDUCTION,
	PROP_REACTION_PARAM_COEFF,
	PROP_WEIGHTED_Q_MODE,
	PROP_I_VOP_QUANT_INITIAL_VALUE,
	PROP_P_VOP_QUANT_INITIAL_VALUE,
	PROP_USE_D_QUANT,
	PROP_CLIP_D_QUANT_FRAME,
	PROP_QUANT_MIN,
	PROP_QUANT_MIN_I_VOP_UNDER_RANGE,
	PROP_QUANT_MAX,
	PROP_PARAM_CHANGEABLE,
	PROP_CHANGEABLE_MAX_BITRATE,
	/* MPEG4 */
	PROP_OUT_VOS,
	PROP_OUT_GOV,
	PROP_ASPECT_RATIO_INFO_TYPE,
	PROP_ASPECT_RATIO_INFO_VALUE,
	PROP_VOS_PROFILE_LEVEL_TYPE,
	PROP_VOS_PROFILE_LEVEL_VALUE,
	PROP_OUT_VISUAL_OBJECT_IDENTIFIER,
	PROP_VISUAL_OBJECT_VERID,
	PROP_VISUAL_OBJECT_PRIORITY,
	PROP_VIDEO_OBJECT_TYPE_INDICATION,
	PROP_OUT_OBJECT_LAYER_IDENTIFIER,
	PROP_VIDEO_OBJECT_LAYER_VERID,
	PROP_VIDEO_OBJECT_LAYER_PRIORITY,
	PROP_ERROR_RESILIENCE_MODE,
	PROP_VIDEO_PACKET_SIZE_MB,
	PROP_VIDEO_PACKET_SIZE_BIT,
	PROP_VIDEO_PACKET_HEADER_EXTENTION,
	PROP_DATA_PARTITIONED,
	PROP_REVERSIBLE_VLC,
	PROP_HIGH_QUALITY,
	PROP_RATECONTROL_VBV_SKIPCHECK_ENABLE,
	PROP_RATECONTROL_VBV_I_VOP_NOSKIP,
	PROP_RATECONTROL_VBV_REMAIN_ZERO_SKIP_ENABLE,
	PROP_RATECONTROL_VBV_BUFFER_UNIT_SIZE,
	PROP_RATECONTROL_VBV_BUFFER_MODE,
	PROP_RATECONTROL_VBV_MAX_SIZE,
	PROP_RATECONTROL_VBV_OFFSET,
	PROP_RATECONTROL_VBV_OFFSET_RATE,
	PROP_QUANT_TYPE,
	PROP_USE_AC_PREDICTION,
	PROP_VOP_MIN_MODE,
	PROP_VOP_MIN_SIZE,
	PROP_INTRA_THR,
	PROP_B_VOP_NUM,
	/* H264 */
	PROP_REF_FRAME_NUM,
	PROP_OUTPUT_FILLER_ENABLE,
	PROP_CLIP_D_QUANT_NEXT_MB,
	PROP_RATECONTROL_CPB_SKIPCHECK_ENABLE,
	PROP_RATECONTROL_CPB_I_VOP_NOSKIP,
	PROP_RATECONTROL_CPB_REMAIN_ZERO_SKIP_ENABLE,
	PROP_RATECONTROL_CPB_BUFFER_UNIT_SIZE,
	PROP_RATECONTROL_CPB_BUFFER_MODE,
	PROP_RATECONTROL_CPB_MAX_SIZE,
	PROP_RATECONTROL_CPB_OFFSET,
	PROP_RATECONTROL_CPB_OFFSET_RATE,
	PROP_INTRA_THR_1,
	PROP_INTRA_THR_2,
	PROP_SAD_INTRA_BIAS,
	PROP_REGULARLY_INSERTED_I_TYPE,
	PROP_CALL_UNIT,
	PROP_USE_SLICE,
	PROP_SLICE_SIZE_MB,
	PROP_SLICE_SIZE_BIT,
	PROP_SLICE_TYPE_VALUE_PATTERN,
	PROP_USE_MB_PARTITION,
	PROP_MB_PARTITION_VECTOR_THR,
	PROP_DEBLOCKING_MODE,
	PROP_USE_DEBLOCKING_FILTER_CONTROL,
	PROP_DEBLOCKING_ALPHA_OFFSET,
	PROP_DEBLOCKING_BETA_OFFSET,	
	PROP_ME_SKIP_MODE,
	PROP_PUT_START_CODE,
	PROP_SEQ_PARAM_SET_ID,
	PROP_PROFILE,
	PROP_CONSTRAINT_SET_FLAG,
	PROP_LEVEL_TYPE,
	PROP_LEVEL_VALUE,
	PROP_OUT_VUI_PARAMETERS,
	PROP_CHROMA_QP_INDEX_OFFSET,
	PROP_CONSTRAINED_INTRA_PRED,
	PROP_LAST
};

#define STREAM_TYPE_H264 "h264"
#define STREAM_TYPE_MPEG4 "mpeg4"
#define STREAM_TYPE_NONE ""

/** 
 * Initializes shvideoenc class
 * @param g_class Gclass
 * @param data user data pointer, unused in the function
 */
static void gst_sh_video_enc_init_class(gpointer g_class, gpointer data);

/** 
 * Initializes SH hardware video encoder
 * @param klass Gstreamer element class
 */
static void gst_sh_video_enc_base_init(gpointer klass);
 
/** 
 * Disposes the encoder
 * @param object Gstreamer element class
 */
static void gst_sh_video_enc_dispose(GObject * object);

/** 
 * Initializes the class for encoder
 * @param klass Gstreamer SH video encoder class
 */
static void gst_sh_video_enc_class_init(GstSHVideoEncClass *klass);

/** 
 * Initializes the encoder
 * @param shvideoenc Gstreamer SH video element
 * @param gklass Gstreamer SH video encode class
 */
static void gst_sh_video_enc_init(GstSHVideoEnc *shvideoenc,
					GstSHVideoEncClass *gklass);
/** 
 * Event handler for encoder sink events
 * @param pad Gstreamer sink pad
 * @param event The Gstreamer event
 * @return returns true if the event can be handled, else false
 */
static gboolean gst_sh_video_enc_sink_event(GstPad * pad, GstEvent * event);

/** 
 * Initializes the encoder sink pad 
 * @param pad Gstreamer sink pad
 * @param caps The capabilities of the data to encode
 * @return returns true if the video capatilies are supported and the video can
 * be decoded, else false
 */
static gboolean gst_sh_video_enc_set_caps(GstPad * pad, GstCaps * caps);

/** 
 * Handles the activation event. Activates the element in pull mode, if it
 * is supported.
 * @param pad Gstreamer sink pad
 * @return returns true if the event is handled without errors, else false
 */
static gboolean	gst_sh_video_enc_activate(GstPad *pad);

/** 
 * Function to start the pad task
 * @param pad Gstreamer sink pad
 * @param active true if the task needs to be started or false to stop the task
 * @return returns true if the event is handled without errors, else false
 */
static gboolean	gst_sh_video_enc_activate_pull(GstPad *pad, gboolean active);

/** 
 * The encoder function and launches the thread if needed
 * @param pad Gstreamer sink pad
 * @param buffer The raw data for encoding.
 * @return returns GST_FLOW_OK if the function runs without errors
 */
static GstFlowReturn gst_sh_video_enc_chain(GstPad *pad, GstBuffer *buffer);

/** 
 * The encoder sink pad task
 * @param enc Gstreamer SH video encoder
 */
static void gst_sh_video_enc_loop(GstSHVideoEnc *enc);

/** 
 * The function will set the encoder properties
 * @param object The object where to get Gstreamer SH video Encoder object
 * @param prop_id The property id
 * @param value The value of the property
 * @param pspec not used in the function
 */
static void gst_sh_video_enc_set_property(GObject *object, guint prop_id, 
					   const GValue *value, 
					   GParamSpec * pspec);

/** 
 * The function will return the values of the encoder properties
 * @param object The object where to get Gstreamer SH video Encoder object
 * @param prop_id The property id
 * @param value The value of the property
 * @param pspec not used in the function
 */
static void gst_sh_video_enc_get_property(GObject * object, guint prop_id,
					  GValue * value, GParamSpec * pspec);

/** 
 * The encoder sink event handler and calls sink pad push event
 * @param pad Gstreamer sink pad
 * @param event Event information
 * @return Returns the value of gst_pad_push_event()
 */
static gboolean gst_sh_video_enc_sink_event(GstPad * pad, GstEvent * event);

/** 
 * Gstreamer source pad query 
 * @param pad Gstreamer source pad
 * @param query Gsteamer query
 * @return Returns the value of gst_pad_query_default
 */
static gboolean gst_sh_video_enc_src_query(GstPad * pad, GstQuery * query);

/** 
 * Callback function for the encoder input
 * @param encoder shcodecs encoder
 * @param user_data Gstreamer SH encoder object
 * @return 0 if encoder should continue. 1 if encoder should pause.
 */
static int gst_sh_video_enc_get_input(SHCodecs_Encoder * encoder, void *user_data);

/** 
 * Callback function for the encoder output
 * @param encoder shcodecs encoder
 * @param data the encoded video frame
 * @param length size the encoded video frame buffer
 * @param user_data Gstreamer SH encoder object
 * @return 0 if encoder should continue. 1 if encoder should pause.
 */
static int gst_sh_video_enc_write_output(SHCodecs_Encoder * encoder,
					unsigned char *data, int length, 
					void *user_data);

/** 
 * GStreamer state handling. We need this for pausing the encoder.
 * @param element GStreamer element
 * @param transition Flag indicating which transition to handle
 * @return GST_STATE_CHANGE_SUCCESS if everything is ok. Otherwise
 *         GST_STATE_CHANGE_FAILURE
 */
static GstStateChangeReturn
gst_sh_video_enc_change_state(GstElement *element, GstStateChange transition);


static void
gst_sh_video_enc_init_class(gpointer g_class, gpointer data)
{
	parent_class = g_type_class_peek_parent(g_class);
	gst_sh_video_enc_class_init((GstSHVideoEncClass *)g_class);
}

GType gst_sh_video_enc_get_type(void)
{
	static GType object_type = 0;

	if (object_type == 0)
	{
		static const GTypeInfo object_info = 
		{
			sizeof(GstSHVideoEncClass),
			gst_sh_video_enc_base_init,
			NULL,
			gst_sh_video_enc_init_class,
			NULL,
			NULL,
			sizeof(GstSHVideoEnc),
			0,
			(GInstanceInitFunc)gst_sh_video_enc_init
		};
		
		object_type =
			g_type_register_static(GST_TYPE_ELEMENT, 
						"gst-sh-mobile-enc", 
						&object_info, (GTypeFlags)0);
	}
	
	return object_type;
}

static void
gst_sh_video_enc_base_init(gpointer klass)
{
	static const GstElementDetails plugin_details =
		GST_ELEMENT_DETAILS("SH hardware video encoder",
			 "Codec/Encoder/Video",
			 "Encode mpeg-based video stream(mpeg4, h264)",
			 "Johannes Lahti <johannes.lahti@nomovok.com>");
	GstElementClass *element_class = GST_ELEMENT_CLASS(klass);

	gst_element_class_add_pad_template(element_class,
			gst_static_pad_template_get(&enc_src_factory));
	gst_element_class_add_pad_template(element_class,
			gst_static_pad_template_get(&enc_sink_factory));
	gst_element_class_set_details(element_class, &plugin_details);
}

static void
gst_sh_video_enc_dispose(GObject * object)
{
	GstSHVideoEnc *enc = GST_SH_VIDEO_ENC(object);

	if (enc->encoder != NULL)
	{
		shcodecs_encoder_close(enc->encoder);
		enc->encoder = NULL;
	}

	pthread_mutex_destroy(&enc->mutex);
	pthread_mutex_destroy(&enc->cond_mutex);
	pthread_cond_destroy(&enc->thread_condition);

	G_OBJECT_CLASS(parent_class)->dispose(object);
}

static void
gst_sh_video_enc_class_init(GstSHVideoEncClass * klass)
{
	GObjectClass *g_object_class;
	GstElementClass *gst_element_class;

	g_object_class = (GObjectClass *)klass;
	gst_element_class = (GstElementClass *)klass;

	g_object_class->dispose = gst_sh_video_enc_dispose;
	g_object_class->set_property = gst_sh_video_enc_set_property;
	g_object_class->get_property = gst_sh_video_enc_get_property;

	GST_DEBUG_CATEGORY_INIT(gst_sh_mobile_debug, "gst-sh-mobile-enc",
			0, "Encoder for H264/MPEG4 streams");
	
	g_object_class_install_property(g_object_class, PROP_CNTL_FILE,
			g_param_spec_string("cntl-file", 
					     		 "Control file location", 
			                     "Location of the file including encoding parameters", 
				             	 NULL, 
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_STREAM_TYPE,
			g_param_spec_string("stream-type", 
					     		 "Stream type", 
			                     "The type of the stream (h264/mpeg4)", 
				             	 NULL, 
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_WIDTH,
					 g_param_spec_long("width", 
							    "Width", 
							    "Width of the video frame", 
							    0, G_MAXLONG, DEFAULT_WIDTH,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_HEIGHT,
					 g_param_spec_long("height", 
							    "Height", 
							    "Height of the video frame", 
							    0, G_MAXLONG, DEFAULT_HEIGHT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_FRAMERATE,
					 g_param_spec_long("framerate", 
							    "Framerate", 
							    "Framerate of the video stream. Multiply with 10 fe. 30fps => 300", 
							    0, G_MAXLONG, DEFAULT_FRAMERATE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_BITRATE,
					 g_param_spec_long("bitrate", 
							    "Bitrate", 
							    "Bitrate of the video stream", 
							    0, G_MAXLONG, DEFAULT_BITRATE_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_I_VOP_INTERVAL,
					 g_param_spec_long("i-vop-interval", 
							    "I VOP interval", 
							    "", 
							    0, G_MAXLONG, DEFAULT_I_VOP_INTERVAL,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_MV_MODE,
					 g_param_spec_long("mv-mode", 
							    "MV mode", 
							    "", 
							    0, G_MAXLONG, DEFAULT_MV_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_FCODE_FORWARD,
					 g_param_spec_long("fcode-forward", 
							    "I VOP interval", 
							    "", 
							    0, G_MAXLONG, DEFAULT_FCODE_FORWARD,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SEARCH_MODE,
					 g_param_spec_long("search-mode", 
							    "Search mode", 
							    "", 
							    0, G_MAXLONG, DEFAULT_SEARCH_MODE_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SEARCH_TIME_FIXED,
					 g_param_spec_long("search-time-fixed", 
							    "Search time fixed", 
							    "", 
							    0, G_MAXLONG, DEFAULT_SEARCH_TIME_FIXED,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_SKIP_ENABLE,
					 g_param_spec_long("ratecontrol-skip-enable", 
							    "Rate control skip enable", 
							    "", 
							    0, G_MAXLONG, DEFAULT_RATECONTROL_SKIP_ENABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_USE_PREVQUANT,
					 g_param_spec_long("ratecontrol-use-prevquant", 
							    "Rate control use prev quant", 
							    "", 
							    0, G_MAXLONG, DEFAULT_RATECONTROL_USE_PREVQUANT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_RESPECT_TYPE,
					 g_param_spec_long("ratecontrol-respect-type", 
							    "Rate control respect type", 
							    "", 
							    0, G_MAXLONG, DEFAULT_RATECONTROL_RESPECT_TYPE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_INTRA_THR_CHANGEABLE,
					 g_param_spec_long("ratecontrol-intra-thr-changeable", 
							    "Rate control intra THR changeable", 
							    "", 
							    0, G_MAXLONG, DEFAULT_RATECONTROL_INTRA_THR_CHANGEABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CONTROL_BITRATE_LENGTH,
					 g_param_spec_long("control-bitrate-length", 
							    "Control bitrate length", 
							    "", 
							    0, G_MAXLONG, DEFAULT_CONTROL_BITRATE_LENGTH,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_INTRA_MACROBLOCK_REFRESH_CYCLE,
					 g_param_spec_long("intra-macroblock-refresh-cycle", 
							    "Intra macroblock refresh cycle", 
							    "", 
							    0, G_MAXLONG, DEFAULT_INTRA_MACROBLOCK_REFRESH_CYCLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_FORMAT,
					 g_param_spec_long("video-format", 
							    "Video format", 
							    "", 
							    0, G_MAXLONG, DEFAULT_VIDEO_FORMAT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_FRAME_NUM_RESOLUTION,
					 g_param_spec_long("frame-num-resolution", 
							    "Frame number resolution", 
							    "", 
							    0, G_MAXLONG, DEFAULT_FRAME_NUM_RESOLUTION,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_NOISE_REDUCTION,
					 g_param_spec_long("noise-reduction", 
							    "Noise reduction", 
							    "", 
							    0, G_MAXLONG, DEFAULT_NOISE_REDUCTION,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_REACTION_PARAM_COEFF,
					 g_param_spec_long("reaction-param-coeff", 
							    "Reaction parameter coefficient", 
							    "", 
							    0, G_MAXLONG, DEFAULT_REACTION_PARAM_COEFF,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_WEIGHTED_Q_MODE,
					 g_param_spec_long("weighted-q-mode", 
							    "Weighted Q-mode", 
							    "", 
							    0, G_MAXLONG, DEFAULT_WEIGHTED_Q_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_I_VOP_QUANT_INITIAL_VALUE,
					 g_param_spec_ulong("i-vop-quant-initial-value", 
							    "I-VOP quantization intitial value", 
							    "", 
							    0, G_MAXULONG, DEFAULT_I_VOP_QUANT_INITIAL_VALUE_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_P_VOP_QUANT_INITIAL_VALUE,
					 g_param_spec_ulong("p-vop-quant-initial-value", 
							    "P-VOP quantization intitial value", 
							    "", 
							    0, G_MAXULONG, DEFAULT_P_VOP_QUANT_INITIAL_VALUE_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_USE_D_QUANT,
					 g_param_spec_ulong("use-d-quant", 
							    "Use D-quantization", 
							    "", 
							    0, G_MAXULONG, DEFAULT_USE_D_QUANT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CLIP_D_QUANT_FRAME,
					 g_param_spec_ulong("clip-d-quant-frame", 
							     "Clip D-quantized frame", 
							    "", 
							    0, G_MAXULONG, DEFAULT_USE_D_QUANT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_QUANT_MIN,
					 g_param_spec_ulong("quant-min", 
							    "Minimum quantization", 
							    "", 
							    0, G_MAXULONG, DEFAULT_QUANT_MIN_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_QUANT_MIN_I_VOP_UNDER_RANGE,
					 g_param_spec_ulong("quant-min-i-vop-under-range", 
							    "", 
							    "", 
							    0, G_MAXULONG, DEFAULT_QUANT_MIN_I_VOP_UNDER_RANGE_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_QUANT_MAX,
					 g_param_spec_ulong("quant-max", 
							    "Maximum quantization", 
							    "", 
							     0, G_MAXULONG, DEFAULT_QUANT_MAX_H264,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_PARAM_CHANGEABLE,
					 g_param_spec_ulong("param-changeable", 
							    "", 
							    "", 
							    0, G_MAXULONG, DEFAULT_PARAM_CHANGEABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CHANGEABLE_MAX_BITRATE,
					 g_param_spec_ulong("changeable-max-bitrate", 
							    "Maximum changeable bitrate", 
							    "", 
							    0, G_MAXULONG, DEFAULT_CHANGEABLE_MAX_BITRATE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

        //MPEG4

	g_object_class_install_property(g_object_class, PROP_OUT_VOS,
					 g_param_spec_ulong("out-vos", 
							    "Out VOS", 
							    "", 
							    0, G_MAXULONG, DEFAULT_OUT_VOS,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_OUT_GOV,
					 g_param_spec_ulong("out-gov", 
							    "Out GOV", 
							    "", 
							    0, G_MAXULONG, DEFAULT_OUT_GOV,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_ASPECT_RATIO_INFO_TYPE,
					 g_param_spec_ulong("aspect-ratio-info-type", 
							    "Aspect ration info type", 
							    "", 
							    0, G_MAXULONG, DEFAULT_ASPECT_RATIO_INFO_TYPE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_ASPECT_RATIO_INFO_VALUE,
					 g_param_spec_ulong("aspect-ratio-info-value", 
							    "Aspect ration info value", 
							    "", 
							    0, G_MAXULONG, DEFAULT_ASPECT_RATIO_INFO_VALUE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VOS_PROFILE_LEVEL_TYPE,
					 g_param_spec_ulong("vos-profile-level-type", 
							    "VOS proile level type", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VOS_PROFILE_LEVEL_TYPE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VOS_PROFILE_LEVEL_VALUE,
					 g_param_spec_ulong("vos-profile-level-value", 
							    "VOS proile level value", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VOS_PROFILE_LEVEL_VALUE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_OUT_VISUAL_OBJECT_IDENTIFIER,
					 g_param_spec_ulong("out-visual-object-identifier", 
							    "Out visual object identifier", 
							    "", 
							    0, G_MAXULONG, DEFAULT_OUT_VISUAL_OBJECT_IDENTIFIER,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VISUAL_OBJECT_VERID,
					 g_param_spec_ulong("visual-object-verid", 
							    "Visual object verid", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VISUAL_OBJECT_VERID,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VISUAL_OBJECT_PRIORITY,
					 g_param_spec_ulong("visual-object-priority", 
							    "Visual object priority", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VISUAL_OBJECT_PRIORITY,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_OBJECT_TYPE_INDICATION,
					 g_param_spec_ulong("visual-object-type-indication", 
							    "Visual object type indication", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VIDEO_OBJECT_TYPE_INDICATION,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_OUT_OBJECT_LAYER_IDENTIFIER,
					 g_param_spec_ulong("out-object-layer-identifier", 
							    "Out object layer identifier", 
							    "", 
							    0, G_MAXULONG, DEFAULT_OUT_OBJECT_LAYER_IDENTIFIER,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_OBJECT_LAYER_VERID,
					 g_param_spec_ulong("video-object-layer-verid", 
							    "Video object layer verid", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VIDEO_OBJECT_LAYER_VERID,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_OBJECT_LAYER_PRIORITY,
					 g_param_spec_ulong("video-object-layer-priority", 
							    "Video object layer priority", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VIDEO_OBJECT_LAYER_PRIORITY,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property(g_object_class, PROP_ERROR_RESILIENCE_MODE,
					 g_param_spec_ulong("error-resilience-mode", 
							    "Error resilience mode", 
							    "", 
							    0, G_MAXULONG, DEFAULT_ERROR_RESILIENCE_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_PACKET_SIZE_MB,
					 g_param_spec_ulong("video-packet-size-mb", 
							    "Video packet size MB", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VIDEO_PACKET_SIZE_MB,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_PACKET_SIZE_BIT,
					 g_param_spec_ulong("video-packet-size-bit", 
							    "Video packet size bit", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VIDEO_PACKET_SIZE_BIT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VIDEO_PACKET_HEADER_EXTENTION,
					 g_param_spec_ulong("video-packet-header-extention", 
							    "Video packet header extention", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VIDEO_PACKET_HEADER_EXTENTION,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_DATA_PARTITIONED,
					 g_param_spec_ulong("data-partitioned", 
							    "Data partitioned", 
							    "", 
							    0, G_MAXULONG, DEFAULT_DATA_PARTITIONED,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_REVERSIBLE_VLC,
					 g_param_spec_ulong("reversible-vlc", 
							    "Reversible VLC", 
							    "", 
							    0, G_MAXULONG, DEFAULT_REVERSIBLE_VLC,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_HIGH_QUALITY,
					 g_param_spec_ulong("high-quality", 
							    "High quality", 
							    "", 
							    0, G_MAXULONG, DEFAULT_HIGH_QUALITY,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_SKIPCHECK_ENABLE,
					 g_param_spec_ulong("ratecontrol-vbv-skipcheck-enable", 
							    "Rate control VBV skip check enable", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_SKIPCHECK_ENABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_I_VOP_NOSKIP,
					 g_param_spec_ulong("ratecontrol-vbv-i-vop-noskip", 
							    "Rate control VBV I-VOP no skip", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_I_VOP_NOSKIP,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_REMAIN_ZERO_SKIP_ENABLE,
					 g_param_spec_ulong("ratecontrol-vbv-remain-zero-skip-enable", 
							    "Rate control VBV remain zero skip enable", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_REMAIN_ZERO_SKIP_ENABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_BUFFER_UNIT_SIZE,
					 g_param_spec_ulong("ratecontrol-vbv-buffer-unit-size", 
							    "Rate control VBV buffer unit size", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_BUFFER_UNIT_SIZE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_BUFFER_MODE,
					 g_param_spec_ulong("ratecontrol-vbv-buffer-mode", 
							    "Rate control VBV buffer mode", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_BUFFER_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_MAX_SIZE,
					 g_param_spec_ulong("ratecontrol-vbv-max-size", 
							    "Rate control VBV max size", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_MAX_SIZE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_OFFSET,
					 g_param_spec_ulong("ratecontrol-vbv-offset", 
							    "Rate control VBV offset", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_OFFSET,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_VBV_OFFSET_RATE,
					 g_param_spec_ulong("ratecontrol-vbv-offset-rate", 
							    "Rate control VBV offset rate", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_VBV_OFFSET_RATE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	g_object_class_install_property(g_object_class, PROP_QUANT_TYPE,
					 g_param_spec_ulong("quant-type", 
							    "Quantization type", 
							    "", 
							    0, G_MAXULONG, DEFAULT_QUANT_TYPE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_USE_AC_PREDICTION,
					 g_param_spec_ulong("use-ac-prediction", 
							    "Use AC prediction", 
							    "", 
							    0, G_MAXULONG, DEFAULT_USE_AC_PREDICTION,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VOP_MIN_MODE,
					 g_param_spec_ulong("vop-min-mode", 
							    "VOP min mode", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VOP_MIN_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_VOP_MIN_SIZE,
					 g_param_spec_ulong("vop-min-size", 
							    "VOP min size", 
							    "", 
							    0, G_MAXULONG, DEFAULT_VOP_MIN_SIZE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_INTRA_THR,
					 g_param_spec_ulong("intra-thr", 
							    "Intra THR", 
							    "", 
							    0, G_MAXULONG, DEFAULT_INTRA_THR,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_B_VOP_NUM,
					 g_param_spec_ulong("b-vop-num", 
							    "B-VOP num", 
							    "", 
							    0, G_MAXULONG, DEFAULT_B_VOP_NUM,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_REF_FRAME_NUM,
					 g_param_spec_int("ref-frame-num", 
							    "Ref frame num", 
							    "", 
							    0, G_MAXINT, DEFAULT_REF_FRAME_NUM,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_OUTPUT_FILLER_ENABLE,
					 g_param_spec_int("output-filler-enable", 
							    "Output filler enable", 
							    "", 
							    0, G_MAXINT, DEFAULT_OUTPUT_FILLER_ENABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CLIP_D_QUANT_NEXT_MB,
					 g_param_spec_ulong("clip-d-quant-next-mb", 
							    "Clip D-quant next mb", 
							    "", 
							    0, G_MAXULONG, DEFAULT_CLIP_D_QUANT_NEXT_MB,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_SKIPCHECK_ENABLE,
					 g_param_spec_ulong("clip-ratecontrol-cpb-skipcheck-enable", 
							    "Ratecontrol CPB skipcheck enable", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_SKIPCHECK_ENABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_I_VOP_NOSKIP,
					 g_param_spec_ulong("clip-ratecontrol-cpb-i-vop-noskip", 
							    "Ratecontrol CPB I-VOP noskip", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_I_VOP_NOSKIP,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_REMAIN_ZERO_SKIP_ENABLE,
					 g_param_spec_ulong("clip-ratecontrol-cpb-remain-zero-skip-enable", 
							    "Ratecontrol CPB remain zero skip enable", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_REMAIN_ZERO_SKIP_ENABLE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_BUFFER_UNIT_SIZE,
					 g_param_spec_ulong("clip-ratecontrol-cpb-buffer-unit-size", 
							    "Ratecontrol CPB buffer unit size", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_BUFFER_UNIT_SIZE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_BUFFER_MODE,
					 g_param_spec_ulong("clip-ratecontrol-cpb-buffer-mode", 
							    "Ratecontrol CPB buffer mode", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_BUFFER_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_MAX_SIZE,
					 g_param_spec_ulong("clip-ratecontrol-cpb-max-size", 
							    "Ratecontrol CPB max size", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_MAX_SIZE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_OFFSET,
					 g_param_spec_ulong("clip-ratecontrol-cpb-offset", 
							    "Ratecontrol CPB offset", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_OFFSET,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_RATECONTROL_CPB_OFFSET_RATE,
					 g_param_spec_ulong("clip-ratecontrol-cpb-offset-rate", 
							    "Ratecontrol CPB offset rate", 
							    "", 
							    0, G_MAXULONG, DEFAULT_RATECONTROL_CPB_OFFSET_RATE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_INTRA_THR_1,
					 g_param_spec_ulong("intra-thr-1", 
							    "Intra THR 1", 
							    "", 
							    0, G_MAXULONG, DEFAULT_INTRA_THR_1,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_INTRA_THR_2,
					 g_param_spec_ulong("intra-thr-2", 
							    "Intra THR 2", 
							    "", 
							    0, G_MAXULONG, DEFAULT_INTRA_THR_2,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SAD_INTRA_BIAS,
					 g_param_spec_ulong("sad-intra-bias", 
							    "SAD intra bias", 
							    "", 
							    0, G_MAXULONG, DEFAULT_SAD_INTRA_BIAS,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_REGULARLY_INSERTED_I_TYPE,
					 g_param_spec_ulong("regularly-inserted-i-type", 
							    "Regularly inserted I-type", 
							    "", 
							    0, G_MAXULONG, DEFAULT_REGULARLY_INSERTED_I_TYPE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CALL_UNIT,
					 g_param_spec_ulong("call-unit", 
							    "Call unit", 
							    "", 
							    0, G_MAXULONG, DEFAULT_CALL_UNIT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_USE_SLICE,
					 g_param_spec_ulong("use-slice", 
							    "", 
							    "", 
							    0, G_MAXULONG, DEFAULT_USE_SLICE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SLICE_SIZE_MB,
					 g_param_spec_ulong("slice-size-mb", 
							    "Slice size MB", 
							    "", 
							     0, G_MAXULONG, DEFAULT_SLICE_SIZE_MB,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SLICE_SIZE_BIT,
					 g_param_spec_ulong("slice-size-bit", 
							    "Slice size bit", 
							    "", 
							     0, G_MAXULONG, DEFAULT_SLICE_SIZE_BIT,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SLICE_TYPE_VALUE_PATTERN,
					 g_param_spec_ulong("slice-size-type-value-pattern", 
							    "Slice size type value pattern", 
							    "", 
							     0, G_MAXULONG, DEFAULT_SLICE_TYPE_VALUE_PATTERN,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


	g_object_class_install_property(g_object_class, PROP_USE_MB_PARTITION,
					 g_param_spec_ulong("use-mb-partition", 
							    "Use MB partition", 
							    "", 
							    0, G_MAXULONG, DEFAULT_USE_MB_PARTITION,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_MB_PARTITION_VECTOR_THR,
					 g_param_spec_ulong("mb-partition-vector-thr", 
							    "MB partition vector THR", 
							    "", 
							    0, G_MAXULONG, DEFAULT_MB_PARTITION_VECTOR_THR,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_DEBLOCKING_MODE,
					 g_param_spec_ulong("deblocking-mode", 
							    "Deblocking mode", 
							    "", 
							    0, G_MAXULONG, DEFAULT_DEBLOCKING_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_USE_DEBLOCKING_FILTER_CONTROL,
					 g_param_spec_ulong("use-deblocking-filter-control", 
							    "Use deblocking filter control", 
							    "", 
							    0, G_MAXULONG, DEFAULT_USE_DEBLOCKING_FILTER_CONTROL,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_DEBLOCKING_ALPHA_OFFSET,
					 g_param_spec_long("deblocking-alpha-offset", 
							    "Deblocking alpha offset", 
							    "", 
							    0, G_MAXLONG, DEFAULT_DEBLOCKING_ALPHA_OFFSET,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_DEBLOCKING_BETA_OFFSET,
					 g_param_spec_long("deblocking-beta-offset", 
							    "Deblocking beta offset", 
							    "", 
							    0, G_MAXLONG, DEFAULT_DEBLOCKING_BETA_OFFSET,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));


	g_object_class_install_property(g_object_class, PROP_ME_SKIP_MODE,
					 g_param_spec_ulong("me-skip-mode", 
							    "ME skip mode", 
							    "", 
							    0, G_MAXULONG, DEFAULT_ME_SKIP_MODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_PUT_START_CODE,
					 g_param_spec_ulong("put-start-code", 
							    "put start code", 
							    "", 
							    0, G_MAXULONG, DEFAULT_PUT_START_CODE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_SEQ_PARAM_SET_ID,
					 g_param_spec_ulong("seq-param-set-id", 
							     "Seq param set id", 
							    "", 
							    0, G_MAXULONG, DEFAULT_SEQ_PARAM_SET_ID,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_PROFILE,
					 g_param_spec_ulong("profile", 
							    "Profile", 
							    "", 
							    0, G_MAXULONG, DEFAULT_PROFILE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CONSTRAINT_SET_FLAG,
					 g_param_spec_ulong("constraint-set-flag", 
							    "Constraint set flag", 
							    "", 
							    0, G_MAXULONG, DEFAULT_CONSTRAINT_SET_FLAG,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_LEVEL_TYPE,
					 g_param_spec_ulong("level-type", 
							    "Level type", 
							    "", 
							    0, G_MAXULONG, DEFAULT_LEVEL_TYPE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_LEVEL_VALUE,
					 g_param_spec_ulong("level-value", 
							    "Level value", 
							    "", 
							    0, G_MAXULONG, DEFAULT_LEVEL_VALUE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_OUT_VUI_PARAMETERS,
					 g_param_spec_ulong("out-vui-parameters", 
							    "Out VUI parameters", 
							    "", 
							    0, G_MAXULONG, DEFAULT_OUT_VUI_PARAMETERS,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CHROMA_QP_INDEX_OFFSET,
					 g_param_spec_ulong("chroma-qp-index-offset", 
							    "Chroma QP index offset", 
							    "", 
							    0, G_MAXULONG, DEFAULT_CHROMA_QP_INDEX_OFFSET,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property(g_object_class, PROP_CONSTRAINED_INTRA_PRED,
					 g_param_spec_ulong("constrained-intra-pred", 
							    "Constrained intra pred", 
							    "", 
							    0, G_MAXULONG, DEFAULT_CONSTRAINED_INTRA_PRED,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
	
	gst_element_class->change_state = gst_sh_video_enc_change_state;
}

static void
gst_sh_video_enc_init(GstSHVideoEnc * enc,
		GstSHVideoEncClass * gklass)
{
	GstElementClass *klass = GST_ELEMENT_GET_CLASS(enc);

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	enc->sinkpad = gst_pad_new_from_template(
						gst_element_class_get_pad_template(klass, "sink"), 
						"sink");

	gst_element_add_pad(GST_ELEMENT(enc), enc->sinkpad);

	gst_pad_set_setcaps_function(enc->sinkpad, 
				      gst_sh_video_enc_set_caps);
	gst_pad_set_activate_function(enc->sinkpad, 
 				       gst_sh_video_enc_activate);
	gst_pad_set_activatepull_function(enc->sinkpad,
					   gst_sh_video_enc_activate_pull);
	gst_pad_set_event_function(enc->sinkpad, 
				   gst_sh_video_enc_sink_event);
	gst_pad_set_chain_function(enc->sinkpad, 
				   gst_sh_video_enc_chain);
	enc->srcpad = gst_pad_new_from_template(
					gst_element_class_get_pad_template(klass, "src"), "src");
	gst_pad_use_fixed_caps(enc->srcpad);

	gst_pad_set_query_function(enc->srcpad,
			GST_DEBUG_FUNCPTR(gst_sh_video_enc_src_query));

	gst_element_add_pad(GST_ELEMENT(enc), enc->srcpad);

	enc->encoder = NULL;
	enc->caps_set = FALSE;
	enc->enc_thread = 0;
	enc->buffer_yuv = NULL;
	enc->buffer_cbcr = NULL;

	pthread_mutex_init(&enc->mutex, NULL);
	pthread_mutex_init(&enc->cond_mutex, NULL);
	pthread_cond_init(&enc->thread_condition, NULL);

	enc->format = SHCodecs_Format_NONE;
	enc->out_caps = NULL;
	enc->width = 0;
	enc->height = 0;
	enc->fps_numerator = 0;
	enc->fps_denominator = 0;
	enc->frame_number = 0;

	enc->stream_stopped = FALSE;
	enc->eos = FALSE;

	/* PROPERTIES */
	/* common */
	enc->bitrate = 0;
	enc->i_vop_interval = DEFAULT_I_VOP_INTERVAL;
	enc->mv_mode = DEFAULT_MV_MODE;
	enc->fcode_forward = DEFAULT_FCODE_FORWARD;
	enc->search_mode = 0;
	enc->search_time_fixed = DEFAULT_SEARCH_TIME_FIXED;
	enc->ratecontrol_skip_enable = DEFAULT_RATECONTROL_SKIP_ENABLE;
	enc->ratecontrol_use_prevquant = DEFAULT_RATECONTROL_USE_PREVQUANT;
	enc->ratecontrol_respect_type = DEFAULT_RATECONTROL_RESPECT_TYPE;
	enc->ratecontrol_intra_thr_changeable = DEFAULT_RATECONTROL_INTRA_THR_CHANGEABLE;
	enc->control_bitrate_length = DEFAULT_CONTROL_BITRATE_LENGTH;
	enc->intra_macroblock_refresh_cycle = DEFAULT_INTRA_MACROBLOCK_REFRESH_CYCLE;
	enc->video_format = DEFAULT_VIDEO_FORMAT;
	enc->frame_num_resolution = DEFAULT_FRAME_NUM_RESOLUTION;
	enc->noise_reduction = DEFAULT_NOISE_REDUCTION;
	enc->reaction_param_coeff = DEFAULT_REACTION_PARAM_COEFF;
	enc->weighted_q_mode = DEFAULT_WEIGHTED_Q_MODE;
	enc->i_vop_quant_initial_value = 0;
	enc->p_vop_quant_initial_value = 0;
	enc->use_d_quant = DEFAULT_USE_D_QUANT;
	enc->clip_d_quant_frame = 0;
	enc->quant_min = 0;
	enc->quant_min_i_vop_under_range = 0;
	enc->quant_max = 0;
	enc->param_changeable = DEFAULT_PARAM_CHANGEABLE;
	enc->changeable_max_bitrate = DEFAULT_CHANGEABLE_MAX_BITRATE;
	/* mpeg4 */
	enc->out_vos = DEFAULT_OUT_VOS;
	enc->out_gov = DEFAULT_OUT_GOV;
	enc->aspect_ratio_info_type = DEFAULT_ASPECT_RATIO_INFO_TYPE;
	enc->aspect_ratio_info_value = DEFAULT_ASPECT_RATIO_INFO_VALUE;
	enc->vos_profile_level_type = DEFAULT_VOS_PROFILE_LEVEL_TYPE;
	enc->vos_profile_level_value = DEFAULT_VOS_PROFILE_LEVEL_VALUE;
	enc->out_visual_object_identifier = DEFAULT_OUT_VISUAL_OBJECT_IDENTIFIER;
	enc->visual_object_verid = DEFAULT_VISUAL_OBJECT_VERID;
	enc->visual_object_priority = DEFAULT_VISUAL_OBJECT_PRIORITY;
	enc->video_object_type_indication = DEFAULT_VIDEO_OBJECT_TYPE_INDICATION;
	enc->out_object_layer_identifier = DEFAULT_OUT_OBJECT_LAYER_IDENTIFIER;
	enc->video_object_layer_verid = DEFAULT_VIDEO_OBJECT_LAYER_VERID;
	enc->video_object_layer_priority = DEFAULT_VIDEO_OBJECT_LAYER_PRIORITY;
	enc->error_resilience_mode = DEFAULT_ERROR_RESILIENCE_MODE;
	enc->video_packet_size_mb = DEFAULT_VIDEO_PACKET_SIZE_MB;
	enc->video_packet_size_bit = DEFAULT_VIDEO_PACKET_SIZE_BIT;
	enc->video_packet_header_extention = DEFAULT_VIDEO_PACKET_HEADER_EXTENTION;
	enc->data_partitioned = DEFAULT_DATA_PARTITIONED;
	enc->reversible_vlc = DEFAULT_REVERSIBLE_VLC;
	enc->high_quality = DEFAULT_HIGH_QUALITY;
	enc->ratecontrol_vbv_skipcheck_enable = DEFAULT_RATECONTROL_VBV_SKIPCHECK_ENABLE;
	enc->ratecontrol_vbv_i_vop_noskip = DEFAULT_RATECONTROL_VBV_I_VOP_NOSKIP;
	enc->ratecontrol_vbv_remain_zero_skip_enable = DEFAULT_RATECONTROL_VBV_REMAIN_ZERO_SKIP_ENABLE;
	enc->ratecontrol_vbv_buffer_unit_size = DEFAULT_RATECONTROL_VBV_BUFFER_UNIT_SIZE;
	enc->ratecontrol_vbv_buffer_mode = DEFAULT_RATECONTROL_VBV_BUFFER_MODE;
	enc->ratecontrol_vbv_max_size = DEFAULT_RATECONTROL_VBV_MAX_SIZE;
	enc->ratecontrol_vbv_offset = DEFAULT_RATECONTROL_VBV_OFFSET;
	enc->ratecontrol_vbv_offset_rate = DEFAULT_RATECONTROL_VBV_OFFSET_RATE;
	enc->quant_type = DEFAULT_QUANT_TYPE;
	enc->use_ac_prediction = DEFAULT_USE_AC_PREDICTION;
	enc->vop_min_mode = DEFAULT_VOP_MIN_MODE;
	enc->vop_min_size = DEFAULT_VOP_MIN_SIZE;
	enc->intra_thr = DEFAULT_INTRA_THR;
	enc->b_vop_num = DEFAULT_B_VOP_NUM;
	/* h264 */
	enc->ref_frame_num = DEFAULT_REF_FRAME_NUM;
	enc->output_filler_enable = DEFAULT_OUTPUT_FILLER_ENABLE;
	enc->clip_d_quant_next_mb = DEFAULT_CLIP_D_QUANT_NEXT_MB;
	enc->ratecontrol_cpb_skipcheck_enable = DEFAULT_RATECONTROL_CPB_SKIPCHECK_ENABLE;
	enc->ratecontrol_cpb_i_vop_noskip = DEFAULT_RATECONTROL_CPB_I_VOP_NOSKIP;
	enc->ratecontrol_cpb_remain_zero_skip_enable = DEFAULT_RATECONTROL_CPB_REMAIN_ZERO_SKIP_ENABLE;
	enc->ratecontrol_cpb_buffer_unit_size = DEFAULT_RATECONTROL_CPB_BUFFER_UNIT_SIZE;
	enc->ratecontrol_cpb_buffer_mode = DEFAULT_RATECONTROL_CPB_BUFFER_MODE;
	enc->ratecontrol_cpb_max_size = DEFAULT_RATECONTROL_CPB_MAX_SIZE;
	enc->ratecontrol_cpb_offset = DEFAULT_RATECONTROL_CPB_OFFSET;
	enc->ratecontrol_cpb_offset_rate = DEFAULT_RATECONTROL_CPB_OFFSET_RATE;
	enc->intra_thr_1 = DEFAULT_INTRA_THR_1;
	enc->intra_thr_2 = DEFAULT_INTRA_THR_2;
	enc->sad_intra_bias = DEFAULT_SAD_INTRA_BIAS;
	enc->regularly_inserted_i_type = DEFAULT_REGULARLY_INSERTED_I_TYPE;
	enc->call_unit = DEFAULT_CALL_UNIT;
	enc->use_slice = DEFAULT_USE_SLICE;
	enc->slice_size_mb = DEFAULT_SLICE_SIZE_MB;
	enc->slice_size_bit = DEFAULT_SLICE_SIZE_BIT;
	enc->slice_type_value_pattern = DEFAULT_SLICE_TYPE_VALUE_PATTERN;
	enc->use_mb_partition = DEFAULT_USE_MB_PARTITION;
	enc->mb_partition_vector_thr = DEFAULT_MB_PARTITION_VECTOR_THR;
	enc->deblocking_mode = DEFAULT_DEBLOCKING_MODE;
	enc->use_deblocking_filter_control = DEFAULT_USE_DEBLOCKING_FILTER_CONTROL;
	enc->deblocking_alpha_offset = DEFAULT_DEBLOCKING_ALPHA_OFFSET;
	enc->deblocking_beta_offset = DEFAULT_DEBLOCKING_BETA_OFFSET;	
	enc->me_skip_mode = DEFAULT_ME_SKIP_MODE;
	enc->put_start_code = DEFAULT_PUT_START_CODE;
	enc->seq_param_set_id = DEFAULT_SEQ_PARAM_SET_ID;
	enc->profile = DEFAULT_PROFILE;
	enc->constraint_set_flag = DEFAULT_CONSTRAINT_SET_FLAG;
	enc->level_type = DEFAULT_LEVEL_TYPE;
	enc->level_value = DEFAULT_LEVEL_VALUE;
	enc->out_vui_parameters = DEFAULT_OUT_VUI_PARAMETERS;
	enc->chroma_qp_index_offset = DEFAULT_CHROMA_QP_INDEX_OFFSET;
	enc->constrained_intra_pred = DEFAULT_CONSTRAINED_INTRA_PRED;
}

static void
gst_sh_video_enc_set_property(GObject * object, guint prop_id,
		const GValue * value, GParamSpec * pspec)
{
	GstSHVideoEnc *enc = GST_SH_VIDEO_ENC(object);
	const gchar* string;
	
	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	switch (prop_id) 
	{
		case PROP_CNTL_FILE:
		{
			strcpy(enc->ainfo.ctrl_file_name_buf,
				g_value_get_string(value));
			break;
		}
	/* COMMON */
		case PROP_STREAM_TYPE:
		{
			string = g_value_get_string(value);

			if (!strcmp(string, STREAM_TYPE_H264))
			{
				enc->format = SHCodecs_Format_H264;
			}
			else if (!strcmp(string, STREAM_TYPE_MPEG4))
			{
				enc->format = SHCodecs_Format_MPEG4;
			}
			break;
		}
		case PROP_WIDTH:
		{
			enc->width = g_value_get_long(value);
			break;
		}
		case PROP_HEIGHT:
		{
			enc->height = g_value_get_long(value);
			break;
		}
		case PROP_FRAMERATE:
		{
			enc->fps_numerator = g_value_get_long(value);
			enc->fps_denominator = 10;
			break;
		}
		case PROP_BITRATE:
		{
			enc->bitrate = g_value_get_long(value);
			break;
		}
		case PROP_I_VOP_INTERVAL:
		{
			enc->i_vop_interval = g_value_get_long(value);
			break;
		}
		case PROP_MV_MODE:
		{
			enc->mv_mode = g_value_get_long(value);
			break;
		}
		case PROP_FCODE_FORWARD:
		{
			enc->fcode_forward = g_value_get_long(value);
			break;
		}
		case PROP_SEARCH_MODE:
		{
			enc->search_mode = g_value_get_long(value);
			break;
		}
		case PROP_SEARCH_TIME_FIXED:
		{
			enc->search_time_fixed = g_value_get_long(value);
			break;
		}
		case PROP_RATECONTROL_SKIP_ENABLE:
		{
			enc->ratecontrol_skip_enable = g_value_get_long(value);
			break;
		}
		case PROP_RATECONTROL_USE_PREVQUANT:
		{
			enc->ratecontrol_use_prevquant = g_value_get_long(value);
			break;
		}
		case PROP_RATECONTROL_RESPECT_TYPE:
		{
			enc->ratecontrol_respect_type = g_value_get_long(value);
			break;
		}
		case PROP_RATECONTROL_INTRA_THR_CHANGEABLE:
		{
			enc->ratecontrol_intra_thr_changeable = g_value_get_long(value);
			break;
		}
		case PROP_CONTROL_BITRATE_LENGTH:
		{
			enc->control_bitrate_length = g_value_get_long(value);
			break;
		}
		case PROP_INTRA_MACROBLOCK_REFRESH_CYCLE:
		{
			enc->intra_macroblock_refresh_cycle = g_value_get_long(value);
			break;
		}
		case PROP_VIDEO_FORMAT:
		{
			enc->video_format = g_value_get_long(value);
			break;
		}
		case PROP_FRAME_NUM_RESOLUTION:
		{
			enc->frame_num_resolution = g_value_get_long(value);
			break;
		}
		case PROP_NOISE_REDUCTION:
		{
			enc->noise_reduction = g_value_get_long(value);
			break;
		}
		case PROP_REACTION_PARAM_COEFF:
		{
			enc->reaction_param_coeff = g_value_get_long(value);
			break;
		}
		case PROP_WEIGHTED_Q_MODE:
		{
			enc->weighted_q_mode = g_value_get_long(value);
			break;
		}
		case PROP_I_VOP_QUANT_INITIAL_VALUE:
		{
			enc->i_vop_quant_initial_value = g_value_get_ulong(value);
			break;
		}
		case PROP_P_VOP_QUANT_INITIAL_VALUE:
		{
			enc->p_vop_quant_initial_value = g_value_get_ulong(value);
			break;
		}
		case PROP_USE_D_QUANT:
		{
			enc->use_d_quant = g_value_get_ulong(value);
			break;
		}
		case PROP_CLIP_D_QUANT_FRAME:
		{
			enc->clip_d_quant_frame = g_value_get_ulong(value);
			break;
		}
		case PROP_QUANT_MIN:
		{
			enc->quant_min = g_value_get_ulong(value);
			break;
		}
		case PROP_QUANT_MIN_I_VOP_UNDER_RANGE:
		{
			enc->quant_min_i_vop_under_range = g_value_get_ulong(value);
			break;
		}
		case PROP_QUANT_MAX:
		{
			enc->quant_max = g_value_get_ulong(value);
			break;
		}
		case PROP_PARAM_CHANGEABLE:
		{
			enc->param_changeable = g_value_get_ulong(value);
			break;
		}
		case PROP_CHANGEABLE_MAX_BITRATE:
		{
			enc->changeable_max_bitrate = g_value_get_ulong(value);
			break;
		}
	/* MPEG4 */
		case PROP_OUT_VOS:
		{
			enc->out_vos = g_value_get_ulong(value);
			break;
		}
		case PROP_OUT_GOV:
		{
			enc->out_gov = g_value_get_ulong(value);
			break;
		}
		case PROP_ASPECT_RATIO_INFO_TYPE:
		{
			enc->aspect_ratio_info_type = g_value_get_ulong(value);
			break;
		}
		case PROP_ASPECT_RATIO_INFO_VALUE:
		{
			enc->aspect_ratio_info_value = g_value_get_ulong(value);
			break;
		}
		case PROP_VOS_PROFILE_LEVEL_TYPE:
		{
			enc->vos_profile_level_type = g_value_get_ulong(value);
			break;
		}
		case PROP_VOS_PROFILE_LEVEL_VALUE:
		{
			enc->vos_profile_level_value = g_value_get_ulong(value);
			break;
		}
		case PROP_OUT_VISUAL_OBJECT_IDENTIFIER:
		{
			enc->out_visual_object_identifier = g_value_get_ulong(value);
			break;
		}
		case PROP_VISUAL_OBJECT_VERID:
		{
			enc->visual_object_verid = g_value_get_ulong(value);
			break;
		}
		case PROP_VISUAL_OBJECT_PRIORITY:
		{
			enc->visual_object_priority = g_value_get_ulong(value);
			break;
		}
		case PROP_VIDEO_OBJECT_TYPE_INDICATION:
		{
			enc->video_object_type_indication = g_value_get_ulong(value);
			break;
		}
		case PROP_OUT_OBJECT_LAYER_IDENTIFIER:
		{
			enc->out_object_layer_identifier = g_value_get_ulong(value);
			break;
		}
		case PROP_VIDEO_OBJECT_LAYER_VERID:
		{
			enc->video_object_layer_verid = g_value_get_ulong(value);
			break;
		}
		case PROP_VIDEO_OBJECT_LAYER_PRIORITY:
		{
			enc->video_object_layer_priority = g_value_get_ulong(value);
			break;
		}
		case PROP_ERROR_RESILIENCE_MODE:
		{
			enc->error_resilience_mode = g_value_get_ulong(value);
			break;
		}
		case PROP_VIDEO_PACKET_SIZE_MB:
		{
			enc->video_packet_size_mb = g_value_get_ulong(value);
			break;
		}
		case PROP_VIDEO_PACKET_SIZE_BIT:
		{
			enc->video_packet_size_bit = g_value_get_ulong(value);
			break;
		}
		case PROP_VIDEO_PACKET_HEADER_EXTENTION:
		{
			enc->video_packet_header_extention = g_value_get_ulong(value);
			break;
		}
		case PROP_DATA_PARTITIONED:
		{
			enc->data_partitioned = g_value_get_ulong(value);
			break;
		}
		case PROP_REVERSIBLE_VLC:
		{
			enc->reversible_vlc = g_value_get_ulong(value);
			break;
		}
		case PROP_HIGH_QUALITY:
		{
			enc->high_quality = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_SKIPCHECK_ENABLE:
		{
			enc->ratecontrol_vbv_skipcheck_enable = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_I_VOP_NOSKIP:
		{
			enc->ratecontrol_vbv_i_vop_noskip = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_REMAIN_ZERO_SKIP_ENABLE:
		{
			enc->ratecontrol_vbv_remain_zero_skip_enable = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_BUFFER_UNIT_SIZE:
		{
			enc->ratecontrol_vbv_buffer_unit_size = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_BUFFER_MODE:
		{
			enc->ratecontrol_vbv_buffer_mode = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_MAX_SIZE:
		{
			enc->ratecontrol_vbv_max_size = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_OFFSET:
		{
			enc->ratecontrol_vbv_offset = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_VBV_OFFSET_RATE:
		{
			enc->ratecontrol_vbv_offset_rate = g_value_get_ulong(value);
			break;
		}
		case PROP_QUANT_TYPE:
		{
			enc->quant_type = g_value_get_ulong(value);
			break;
		}
		case PROP_USE_AC_PREDICTION:
		{
			enc->use_ac_prediction = g_value_get_ulong(value);
			break;
		}
		case PROP_VOP_MIN_MODE:
		{
			enc->vop_min_mode = g_value_get_ulong(value);
			break;
		}
		case PROP_VOP_MIN_SIZE:
		{
			enc->vop_min_size = g_value_get_ulong(value);
			break;
		}
		case PROP_INTRA_THR:
		{
			enc->intra_thr = g_value_get_ulong(value);
			break;
		}
		case PROP_B_VOP_NUM:
		{
			enc->b_vop_num = g_value_get_ulong(value);
			break;
		}
	/* H264 */
		case PROP_REF_FRAME_NUM:
		{
			enc->ref_frame_num = g_value_get_int(value);
			break;
		}
		case PROP_OUTPUT_FILLER_ENABLE:
		{
			enc->output_filler_enable = g_value_get_int(value);
			break;
		}
		case PROP_CLIP_D_QUANT_NEXT_MB:
		{
			enc->clip_d_quant_next_mb = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_SKIPCHECK_ENABLE:
		{
			enc->ratecontrol_cpb_skipcheck_enable = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_I_VOP_NOSKIP:
		{
			enc->ratecontrol_cpb_i_vop_noskip = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_REMAIN_ZERO_SKIP_ENABLE:
		{
			enc->ratecontrol_cpb_remain_zero_skip_enable = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_BUFFER_UNIT_SIZE:
		{
			enc->ratecontrol_cpb_buffer_unit_size = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_BUFFER_MODE:
		{
			enc->ratecontrol_cpb_buffer_mode = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_MAX_SIZE:
		{
			enc->ratecontrol_cpb_max_size = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_OFFSET:
		{
			enc->ratecontrol_cpb_offset = g_value_get_ulong(value);
			break;
		}
		case PROP_RATECONTROL_CPB_OFFSET_RATE:
		{
			enc->ratecontrol_cpb_offset_rate = g_value_get_ulong(value);
			break;
		}
		case PROP_INTRA_THR_1:
		{
			enc->intra_thr_1 = g_value_get_ulong(value);
			break;
		}
		case PROP_INTRA_THR_2:
		{
			enc->intra_thr_2 = g_value_get_ulong(value);
			break;
		}
		case PROP_SAD_INTRA_BIAS:
		{
			enc->sad_intra_bias = g_value_get_ulong(value);
			break;
		}
		case PROP_REGULARLY_INSERTED_I_TYPE:
		{
			enc->regularly_inserted_i_type = g_value_get_ulong(value);
			break;
		}
		case PROP_CALL_UNIT:
		{
			enc->call_unit = g_value_get_ulong(value);
			break;
		}
		case PROP_USE_SLICE:
		{
			enc->use_slice = g_value_get_ulong(value);
			break;
		}
		case PROP_SLICE_SIZE_MB:
		{
			enc->slice_size_mb = g_value_get_ulong(value);
			break;
		}
		case PROP_SLICE_SIZE_BIT:
		{
			enc->slice_size_bit = g_value_get_ulong(value);
			break;
		}
		case PROP_SLICE_TYPE_VALUE_PATTERN:
		{
			enc->slice_type_value_pattern = g_value_get_ulong(value);
			break;
		}
		case PROP_USE_MB_PARTITION:
		{
			enc->use_mb_partition = g_value_get_ulong(value);
			break;
		}
		case PROP_MB_PARTITION_VECTOR_THR:
		{
			enc->mb_partition_vector_thr = g_value_get_ulong(value);
			break;
		}
		case PROP_DEBLOCKING_MODE:
		{
			enc->deblocking_mode = g_value_get_ulong(value);
			break;
		}
		case PROP_USE_DEBLOCKING_FILTER_CONTROL:
		{
			enc->use_deblocking_filter_control = g_value_get_ulong(value);
			break;
		}
		case PROP_DEBLOCKING_ALPHA_OFFSET:
		{
			enc->deblocking_alpha_offset = g_value_get_long(value);
			break;
		}
		case PROP_DEBLOCKING_BETA_OFFSET:
		{
			enc->deblocking_beta_offset = g_value_get_long(value);
			break;
		}	
		case PROP_ME_SKIP_MODE:
		{
			enc->me_skip_mode = g_value_get_ulong(value);
			break;
		}
		case PROP_PUT_START_CODE:
		{
			enc->put_start_code = g_value_get_ulong(value);
			break;
		}
		case PROP_SEQ_PARAM_SET_ID:
		{
			enc->seq_param_set_id = g_value_get_ulong(value);
			break;
		}
		case PROP_PROFILE:
		{
			enc->profile = g_value_get_ulong(value);
			break;
		}
		case PROP_CONSTRAINT_SET_FLAG:
		{
			enc->constraint_set_flag = g_value_get_ulong(value);
			break;
		}
		case PROP_LEVEL_TYPE:
		{
			enc->level_type = g_value_get_ulong(value);
			break;
		}
		case PROP_LEVEL_VALUE:
		{
			enc->level_value = g_value_get_ulong(value);
			break;
		}
		case PROP_OUT_VUI_PARAMETERS:
		{
			enc->out_vui_parameters = g_value_get_ulong(value);
			break;
		}
		case PROP_CHROMA_QP_INDEX_OFFSET:
		{
			enc->chroma_qp_index_offset = g_value_get_ulong(value);
			break;
		}
		case PROP_CONSTRAINED_INTRA_PRED:
		{
			enc->constrained_intra_pred = g_value_get_ulong(value);
			break;
		}
		default:
		{
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, 
							   pspec);
			break;
		}
	}
}

static void
gst_sh_video_enc_get_property(GObject * object, guint prop_id,
		GValue * value, GParamSpec * pspec)
{
	GstSHVideoEnc *enc = GST_SH_VIDEO_ENC(object);

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	switch (prop_id) 
	{
		case PROP_CNTL_FILE:
		{
			g_value_set_string(value, enc->ainfo.ctrl_file_name_buf);
			break;
		}
	/* COMMON */
		case PROP_STREAM_TYPE:
		{
			switch (enc->format)
			{
				case SHCodecs_Format_H264:
				{
					g_value_set_string(value, STREAM_TYPE_H264);
					break;
				}
				case SHCodecs_Format_MPEG4:
				{
					g_value_set_string(value, STREAM_TYPE_MPEG4);
					break;
				}
				case SHCodecs_Format_NONE:
				{
					g_value_set_string(value, STREAM_TYPE_NONE);
					break;
				}
			}
			break;
		}
		case PROP_WIDTH:
		{
			g_value_set_long(value, enc->width);
			break;
		}
		case PROP_HEIGHT:
		{
			g_value_set_long(value, enc->height);
			break;
		}
		case PROP_FRAMERATE:
		{
			g_value_set_long(value, enc->fps_numerator);
			break;
		}
		case PROP_BITRATE:
		{
			g_value_set_long(value, enc->bitrate);
			break;
		}
		case PROP_I_VOP_INTERVAL:
		{
			g_value_set_long(value, enc->i_vop_interval);
			break;
		}
		case PROP_MV_MODE:
		{
			g_value_set_long(value, enc->mv_mode);
			break;
		}
		case PROP_FCODE_FORWARD:
		{
			g_value_set_long(value, enc->fcode_forward);
			break;
		}
		case PROP_SEARCH_MODE:
		{
			g_value_set_long(value, enc->search_mode);
			break;
		}
		case PROP_SEARCH_TIME_FIXED:
		{
			g_value_set_long(value, enc->search_time_fixed);
			break;
		}
		case PROP_RATECONTROL_SKIP_ENABLE:
		{
			g_value_set_long(value, enc->ratecontrol_skip_enable);
			break;
		}
		case PROP_RATECONTROL_USE_PREVQUANT:
		{
			g_value_set_long(value, enc->ratecontrol_use_prevquant);
			break;
		}
		case PROP_RATECONTROL_RESPECT_TYPE:
		{
			g_value_set_long(value, enc->ratecontrol_respect_type);
			break;
		}
		case PROP_RATECONTROL_INTRA_THR_CHANGEABLE:
		{
			g_value_set_long(value, enc->ratecontrol_intra_thr_changeable);
			break;
		}
		case PROP_CONTROL_BITRATE_LENGTH:
		{
			g_value_set_long(value, enc->control_bitrate_length);
			break;
		}
		case PROP_INTRA_MACROBLOCK_REFRESH_CYCLE:
		{
			g_value_set_long(value, enc->intra_macroblock_refresh_cycle);
			break;
		}
		case PROP_VIDEO_FORMAT:
		{
			g_value_set_long(value, enc->video_format);
			break;
		}
		case PROP_FRAME_NUM_RESOLUTION:
		{
			g_value_set_long(value, enc->frame_num_resolution);
			break;
		}
		case PROP_NOISE_REDUCTION:
		{
			g_value_set_long(value, enc->noise_reduction);
			break;
		}
		case PROP_REACTION_PARAM_COEFF:
		{
			g_value_set_long(value, enc->reaction_param_coeff);
			break;
		}
		case PROP_WEIGHTED_Q_MODE:
		{
			g_value_set_long(value, enc->weighted_q_mode);
			break;
		}
		case PROP_I_VOP_QUANT_INITIAL_VALUE:
		{
			g_value_set_ulong(value, enc->i_vop_quant_initial_value);
			break;
		}
		case PROP_P_VOP_QUANT_INITIAL_VALUE:
		{
			g_value_set_ulong(value, enc->p_vop_quant_initial_value);
			break;
		}
		case PROP_USE_D_QUANT:
		{
			g_value_set_ulong(value, enc->use_d_quant);
			break;
		}
		case PROP_CLIP_D_QUANT_FRAME:
		{
			g_value_set_ulong(value, enc->clip_d_quant_frame);
			break;
		}
		case PROP_QUANT_MIN:
		{
			g_value_set_ulong(value, enc->quant_min);
			break;
		}
		case PROP_QUANT_MIN_I_VOP_UNDER_RANGE:
		{
			g_value_set_ulong(value, enc->quant_min_i_vop_under_range);
			break;
		}
		case PROP_QUANT_MAX:
		{
			g_value_set_ulong(value, enc->quant_max);
			break;
		}
		case PROP_PARAM_CHANGEABLE:
		{
			g_value_set_ulong(value, enc->param_changeable);
			break;
		}
		case PROP_CHANGEABLE_MAX_BITRATE:
		{
			g_value_set_ulong(value, enc->changeable_max_bitrate);
			break;
		}
	/* MPEG4 */
		case PROP_OUT_VOS:
		{
			g_value_set_ulong(value, enc->out_vos);
			break;
		}
		case PROP_OUT_GOV:
		{
			g_value_set_ulong(value, enc->out_gov);
			break;
		}
		case PROP_ASPECT_RATIO_INFO_TYPE:
		{
			g_value_set_ulong(value, enc->aspect_ratio_info_type);
			break;
		}
		case PROP_ASPECT_RATIO_INFO_VALUE:
		{
			g_value_set_ulong(value, enc->aspect_ratio_info_value);
			break;
		}
		case PROP_VOS_PROFILE_LEVEL_TYPE:
		{
			g_value_set_ulong(value, enc->vos_profile_level_type);
			break;
		}
		case PROP_VOS_PROFILE_LEVEL_VALUE:
		{
			g_value_set_ulong(value, enc->vos_profile_level_value);
			break;
		}
		case PROP_OUT_VISUAL_OBJECT_IDENTIFIER:
		{
			g_value_set_ulong(value, enc->out_visual_object_identifier);
			break;
		}
		case PROP_VISUAL_OBJECT_VERID:
		{
			g_value_set_ulong(value, enc->visual_object_verid);
			break;
		}
		case PROP_VISUAL_OBJECT_PRIORITY:
		{
			g_value_set_ulong(value, enc->visual_object_priority);
			break;
		}
		case PROP_VIDEO_OBJECT_TYPE_INDICATION:
		{
			g_value_set_ulong(value, enc->video_object_type_indication);
			break;
		}
		case PROP_OUT_OBJECT_LAYER_IDENTIFIER:
		{
			g_value_set_ulong(value, enc->out_object_layer_identifier);
			break;
		}
		case PROP_VIDEO_OBJECT_LAYER_VERID:
		{
			g_value_set_ulong(value, enc->video_object_layer_verid);
			break;
		}
		case PROP_VIDEO_OBJECT_LAYER_PRIORITY:
		{
			g_value_set_ulong(value, enc->video_object_layer_priority);
			break;
		}
		case PROP_ERROR_RESILIENCE_MODE:
		{
			g_value_set_ulong(value, enc->error_resilience_mode);
			break;
		}
		case PROP_VIDEO_PACKET_SIZE_MB:
		{
			g_value_set_ulong(value, enc->video_packet_size_mb);
			break;
		}
		case PROP_VIDEO_PACKET_SIZE_BIT:
		{
			g_value_set_ulong(value, enc->video_packet_size_bit);
			break;
		}
		case PROP_VIDEO_PACKET_HEADER_EXTENTION:
		{
			g_value_set_ulong(value, enc->video_packet_header_extention);
			break;
		}
		case PROP_DATA_PARTITIONED:
		{
			g_value_set_ulong(value, enc->data_partitioned);
			break;
		}
		case PROP_REVERSIBLE_VLC:
		{
			g_value_set_ulong(value, enc->reversible_vlc);
			break;
		}
		case PROP_HIGH_QUALITY:
		{
			g_value_set_ulong(value, enc->high_quality);
			break;
		}
		case PROP_RATECONTROL_VBV_SKIPCHECK_ENABLE:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_skipcheck_enable);
			break;
		}
		case PROP_RATECONTROL_VBV_I_VOP_NOSKIP:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_i_vop_noskip);
			break;
		}
		case PROP_RATECONTROL_VBV_REMAIN_ZERO_SKIP_ENABLE:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_remain_zero_skip_enable);
			break;
		}
		case PROP_RATECONTROL_VBV_BUFFER_UNIT_SIZE:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_buffer_unit_size);
			break;
		}
		case PROP_RATECONTROL_VBV_BUFFER_MODE:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_buffer_mode);
			break;
		}
		case PROP_RATECONTROL_VBV_MAX_SIZE:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_max_size);
			break;
		}
		case PROP_RATECONTROL_VBV_OFFSET:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_offset);
			break;
		}
		case PROP_RATECONTROL_VBV_OFFSET_RATE:
		{
			g_value_set_ulong(value, enc->ratecontrol_vbv_offset_rate);
			break;
		}
		case PROP_QUANT_TYPE:
		{
			g_value_set_ulong(value, enc->quant_type);
			break;
		}
		case PROP_USE_AC_PREDICTION:
		{
			g_value_set_ulong(value, enc->use_ac_prediction);
			break;
		}
		case PROP_VOP_MIN_MODE:
		{
			g_value_set_ulong(value, enc->vop_min_mode);
			break;
		}
		case PROP_VOP_MIN_SIZE:
		{
			g_value_set_ulong(value, enc->vop_min_size);
			break;
		}
		case PROP_INTRA_THR:
		{
			g_value_set_ulong(value, enc->intra_thr);
			break;
		}
		case PROP_B_VOP_NUM:
		{
			g_value_set_ulong(value, enc->b_vop_num);
			break;
		}
	/* H264 */
		case PROP_REF_FRAME_NUM:
		{
			g_value_set_int(value, enc->ref_frame_num);
			break;
		}
		case PROP_OUTPUT_FILLER_ENABLE:
		{
			g_value_set_int(value, enc->output_filler_enable);
			break;
		}
		case PROP_CLIP_D_QUANT_NEXT_MB:
		{
			g_value_set_ulong(value, enc->clip_d_quant_next_mb);
			break;
		}
		case PROP_RATECONTROL_CPB_SKIPCHECK_ENABLE:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_skipcheck_enable);
			break;
		}
		case PROP_RATECONTROL_CPB_I_VOP_NOSKIP:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_i_vop_noskip);
			break;
		}
		case PROP_RATECONTROL_CPB_REMAIN_ZERO_SKIP_ENABLE:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_remain_zero_skip_enable);
			break;
		}
		case PROP_RATECONTROL_CPB_BUFFER_UNIT_SIZE:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_buffer_unit_size);
			break;
		}
		case PROP_RATECONTROL_CPB_BUFFER_MODE:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_buffer_mode);
			break;
		}
		case PROP_RATECONTROL_CPB_MAX_SIZE:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_max_size);
			break;
		}
		case PROP_RATECONTROL_CPB_OFFSET:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_offset);
			break;
		}
		case PROP_RATECONTROL_CPB_OFFSET_RATE:
		{
			g_value_set_ulong(value, enc->ratecontrol_cpb_offset_rate);
			break;
		}
		case PROP_INTRA_THR_1:
		{
			g_value_set_ulong(value, enc->intra_thr_1);
			break;
		}
		case PROP_INTRA_THR_2:
		{
			g_value_set_ulong(value, enc->intra_thr_2);
			break;
		}
		case PROP_SAD_INTRA_BIAS:
		{
			g_value_set_ulong(value, enc->sad_intra_bias);
			break;
		}
		case PROP_REGULARLY_INSERTED_I_TYPE:
		{
			g_value_set_ulong(value, enc->regularly_inserted_i_type);
			break;
		}
		case PROP_CALL_UNIT:
		{
			g_value_set_ulong(value, enc->call_unit);
			break;
		}
		case PROP_USE_SLICE:
		{
			g_value_set_ulong(value, enc->use_slice);
			break;
		}
		case PROP_SLICE_SIZE_MB:
		{
			g_value_set_ulong(value, enc->slice_size_mb);
			break;
		}
		case PROP_SLICE_SIZE_BIT:
		{
			g_value_set_ulong(value, enc->slice_size_bit);
			break;
		}
		case PROP_SLICE_TYPE_VALUE_PATTERN:
		{
			g_value_set_ulong(value, enc->slice_type_value_pattern);
			break;
		}
		case PROP_USE_MB_PARTITION:
		{
			g_value_set_ulong(value, enc->use_mb_partition);
			break;
		}
		case PROP_MB_PARTITION_VECTOR_THR:
		{
			g_value_set_ulong(value, enc->mb_partition_vector_thr);
			break;
		}
		case PROP_DEBLOCKING_MODE:
		{
			g_value_set_ulong(value, enc->deblocking_mode);
			break;
		}
		case PROP_USE_DEBLOCKING_FILTER_CONTROL:
		{
			g_value_set_ulong(value, enc->use_deblocking_filter_control);
			break;
		}
		case PROP_DEBLOCKING_ALPHA_OFFSET:
		{
			g_value_set_long(value, enc->deblocking_alpha_offset);
			break;
		}
		case PROP_DEBLOCKING_BETA_OFFSET:
		{
			g_value_set_long(value, enc->deblocking_beta_offset);
			break;
		}	
		case PROP_ME_SKIP_MODE:
		{
			g_value_set_ulong(value, enc->me_skip_mode);
			break;
		}
		case PROP_PUT_START_CODE:
		{
			g_value_set_ulong(value, enc->put_start_code);
			break;
		}
		case PROP_SEQ_PARAM_SET_ID:
		{
			g_value_set_ulong(value, enc->seq_param_set_id);
			break;
		}
		case PROP_PROFILE:
		{
			g_value_set_ulong(value, enc->profile);
			break;
		}
		case PROP_CONSTRAINT_SET_FLAG:
		{
			g_value_set_ulong(value, enc->constraint_set_flag);
			break;
		}
		case PROP_LEVEL_TYPE:
		{
			g_value_set_ulong(value, enc->level_type);
			break;
		}
		case PROP_LEVEL_VALUE:
		{
			g_value_set_ulong(value, enc->level_value);
			break;
		}
		case PROP_OUT_VUI_PARAMETERS:
		{
			g_value_set_ulong(value, enc->out_vui_parameters);
			break;
		}
		case PROP_CHROMA_QP_INDEX_OFFSET:
		{
			g_value_set_ulong(value, enc->chroma_qp_index_offset);
			break;
		}
		case PROP_CONSTRAINED_INTRA_PRED:
		{
			g_value_set_ulong(value, enc->constrained_intra_pred);
			break;
		}
		default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static gboolean 
gst_sh_video_enc_sink_event(GstPad * pad, GstEvent * event)
{
	GstSHVideoEnc *enc = (GstSHVideoEnc *)(GST_OBJECT_PARENT(pad));  

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) 
	{
		enc->eos = TRUE;
	}

	return gst_pad_push_event(enc->srcpad, event);
}

static gboolean
gst_sh_video_enc_set_caps(GstPad * pad, GstCaps * caps)
{
	gboolean ret;
	GstStructure *structure;
	GstSHVideoEnc *enc = (GstSHVideoEnc *)(GST_OBJECT_PARENT(pad));

    if (enc->caps_set)
	{
		return TRUE;
	}

	ret = TRUE;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	// get input size
	structure = gst_caps_get_structure(caps, 0);
	ret = gst_structure_get_int(structure, "width", &enc->width);
	ret &= gst_structure_get_int(structure, "height", &enc->height);
	ret &= gst_structure_get_fraction(structure, "framerate", 
						 &enc->fps_numerator, 
						 &enc->fps_denominator);

	if (!ret) 
	{
		return ret;
	}

	gst_sh_video_enc_read_src_caps(enc);
	gst_sh_video_enc_init_encoder(enc);

	if (!gst_caps_is_any(enc->out_caps))
	{
		ret = gst_sh_video_enc_set_src_caps(enc);
	}
		
	if (ret) 
	{
		enc->caps_set = TRUE;
	}

	return ret;
}

void
gst_sh_video_enc_read_sink_caps(GstSHVideoEnc * enc)
{
	GstStructure *structure;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	// get the caps of the previous element in chain
	enc->out_caps = gst_pad_peer_get_caps(enc->sinkpad);
	
	if (!gst_caps_is_any(enc->out_caps))
	{
		structure = gst_caps_get_structure(enc->out_caps, 0);
        if (!enc->width)
		{
	    	gst_structure_get_int(structure, "width", &enc->width);
		}
        if (!enc->height)
		{
		    gst_structure_get_int(structure, "height", &enc->height);
		}
        if (!enc->fps_numerator)
		{
		    gst_structure_get_fraction(structure, "framerate", 
										 &enc->fps_numerator, 
										 &enc->fps_denominator);
		}
	}
}

void
gst_sh_video_enc_read_src_caps(GstSHVideoEnc * enc)
{
	GstStructure *structure;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	// get the caps of the next element in chain
	enc->out_caps = gst_pad_peer_get_caps(enc->srcpad);
	
	if (!gst_caps_is_any(enc->out_caps) && 
		enc->format == SHCodecs_Format_NONE)
	{
		structure = gst_caps_get_structure(enc->out_caps, 0);
		if (!strcmp(gst_structure_get_name(structure), "video/mpeg")) 
		{
			enc->format = SHCodecs_Format_MPEG4;
		}
		else if (!strcmp(gst_structure_get_name(structure), 
				  "video/x-h264")) 
		{
			enc->format = SHCodecs_Format_H264;
		}
	}
}

gboolean
gst_sh_video_enc_set_src_caps(GstSHVideoEnc * enc)
{
	GstCaps* caps = NULL;
	gboolean ret = TRUE;
	
	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	if (enc->format == SHCodecs_Format_MPEG4)
	{
		caps = gst_caps_new_simple("video/mpeg", "width", G_TYPE_INT, 
				enc->width, "height", G_TYPE_INT, 
				enc->height, "framerate", 
				GST_TYPE_FRACTION, enc->fps_numerator, 
				enc->fps_denominator, "mpegversion", 
				G_TYPE_INT, 4, NULL);
	}
	else if (enc->format == SHCodecs_Format_H264)
	{
		caps = gst_caps_new_simple("video/x-h264", "width", G_TYPE_INT, 
				enc->width, "height", G_TYPE_INT, 
				enc->height, "framerate", 
				GST_TYPE_FRACTION, enc->fps_numerator, 
				enc->fps_denominator, NULL);
	}
	else
	{
		GST_ELEMENT_ERROR((GstElement*)enc, CORE, NEGOTIATION,
					("Format undefined."), (NULL));
	}

	if (!gst_pad_set_caps(enc->srcpad, caps))
	{
		GST_ELEMENT_ERROR((GstElement*)enc, CORE, NEGOTIATION,
					("Source pad not linked."), (NULL));
		ret = FALSE;
	}
	if (!gst_pad_set_caps(gst_pad_get_peer(enc->srcpad), caps))
	{
		GST_ELEMENT_ERROR((GstElement*)enc, CORE, NEGOTIATION,
					("Source pad not linked."), (NULL));
		ret = FALSE;
	}
	gst_caps_unref(caps);
	return ret;
}

void
gst_sh_video_enc_init_encoder(GstSHVideoEnc * enc)
{
	gint ret = 0;
	glong fmt = 0;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	if (strlen(enc->ainfo.ctrl_file_name_buf))
	{

		ret = GetFromCtrlFTop((const char *)
				enc->ainfo.ctrl_file_name_buf,
				&enc->ainfo,
				&fmt);
		if (ret < 0) 
		{
			GST_ELEMENT_ERROR((GstElement*)enc, CORE, FAILED,
				  ("Error reading the top of control file."), 
				  (NULL));
		}

		if (enc->format == SHCodecs_Format_NONE)
		{
			enc->format = fmt;
		}

		if (!enc->width)
		{
			enc->width = enc->ainfo.xpic;
		}

		if (!enc->height)
		{
			enc->height = enc->ainfo.ypic;
		}
		if (!enc->fps_numerator)
		{
			enc->fps_numerator = enc->ainfo.frame_rate;
		}

		if (!enc->fps_denominator)
		{
			enc->fps_denominator = 10;
		}
	}

    if (enc->format == SHCodecs_Format_NONE ||
		!(enc->width && enc->height)||
		!(enc->fps_numerator && enc->fps_denominator))
	{
		GST_ELEMENT_ERROR((GstElement*)enc, CORE, FAILED,
			("Key parameters are not set."), 
			("Encoding parameters undefined. Stream format:%d, width:%d, height:%d and framerate: %d/%d",
			 enc->format, enc->width, enc->height,
			 enc->fps_numerator, enc->fps_denominator));
    }

	enc->encoder = shcodecs_encoder_init(enc->width, enc->height, enc->format);

	shcodecs_encoder_set_frame_rate(enc->encoder,
	 	(enc->fps_numerator / enc->fps_denominator) * 10);

	shcodecs_encoder_set_xpic_size(enc->encoder,enc->width);
	shcodecs_encoder_set_ypic_size(enc->encoder,enc->height);

	shcodecs_encoder_set_input_callback(enc->encoder, 
					    gst_sh_video_enc_get_input, enc);
	shcodecs_encoder_set_output_callback(enc->encoder, 
					     gst_sh_video_enc_write_output, enc);

	if (strlen(enc->ainfo.ctrl_file_name_buf))
	{
		ret = GetFromCtrlFtoEncParam(enc->encoder, &enc->ainfo);
		
		if (ret < 0) 
		{
			GST_ELEMENT_ERROR((GstElement*)enc, CORE, FAILED,
				  ("Error reading parameters from control file."), 
				  (NULL));
		}
	}
	else
	{
		if (!gst_sh_video_enc_set_encoding_properties(enc))
		{
			GST_ELEMENT_ERROR((GstElement*)enc, CORE, FAILED,
				  ("Setting SHCodecs encoder properties failed."), 
				  (NULL));
		}		
	}

	GST_DEBUG_OBJECT(enc, "Encoder init: %ldx%ld %ldfps format:%ld",
			 shcodecs_encoder_get_xpic_size(enc->encoder),
			 shcodecs_encoder_get_ypic_size(enc->encoder),
			 shcodecs_encoder_get_frame_rate(enc->encoder) / 10,
			 shcodecs_encoder_get_stream_type(enc->encoder)); 
}

static gboolean
gst_sh_video_enc_activate(GstPad * pad)
{
	gboolean ret;
	GstSHVideoEnc *enc = (GstSHVideoEnc *)(GST_OBJECT_PARENT(pad));

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);
	if (gst_pad_check_pull_range(pad)) 
	{
		GST_LOG_OBJECT(enc, "PULL mode");
		ret = gst_pad_activate_pull(pad, TRUE);
	} 
	else 
	{  
		GST_LOG_OBJECT(enc, "PUSH mode");
		ret = gst_pad_activate_push(pad, TRUE);
	}
	return ret;
}

static GstStateChangeReturn
gst_sh_video_enc_change_state(GstElement *element, GstStateChange transition)
{
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstSHVideoEnc *enc = GST_SH_VIDEO_ENC(element);

	ret = GST_ELEMENT_CLASS(parent_class)->change_state(element, 
							      transition);
	if (ret == GST_STATE_CHANGE_FAILURE)
	{
		return ret;
	}

	switch (transition) 
	{
		case GST_STATE_CHANGE_PAUSED_TO_READY:
		{
			GST_DEBUG_OBJECT(enc, "Stopping encoding.");
			enc->stream_stopped = TRUE;        
			break;
		}
		default:
			break;
	}
	return ret;
}

static GstFlowReturn 
gst_sh_video_enc_chain(GstPad * pad, GstBuffer * buffer)
{
	gint yuv_size, cbcr_size;
	GstSHVideoEnc *enc = (GstSHVideoEnc *)(GST_OBJECT_PARENT(pad));  

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	if (enc->stream_stopped)
	{
		return GST_FLOW_UNEXPECTED;
	}

	if (!enc->caps_set)
	{
		gst_sh_video_enc_read_sink_caps(enc);
		gst_sh_video_enc_read_src_caps(enc);
		gst_sh_video_enc_init_encoder(enc);
		if (!gst_caps_is_any(enc->out_caps))
		{
			if (!gst_sh_video_enc_set_src_caps(enc))
			{
				return GST_FLOW_UNEXPECTED;
			}
		}
		enc->caps_set = TRUE;
	}

	/* If buffers are not empty we'll have to 
	   wait until encoder has consumed data */
	if (enc->buffer_yuv && enc->buffer_cbcr)
	{
		pthread_mutex_lock(&enc->cond_mutex);
		pthread_cond_wait(&enc->thread_condition, &enc->cond_mutex);
		pthread_mutex_unlock(&enc->cond_mutex);
	}

	// Lock mutex while handling the buffers
	pthread_mutex_lock(&enc->mutex);
	yuv_size = enc->width * enc->height;
	cbcr_size = enc->width * enc->height / 2;

	// Check that we have got enough data
	if (GST_BUFFER_SIZE(buffer) != yuv_size + cbcr_size)
	{
		GST_DEBUG_OBJECT(enc, "Not enough data");
		// If we can't continue we can issue EOS
		enc->eos = TRUE;
		gst_pad_push_event(enc->srcpad, gst_event_new_eos());
		return GST_FLOW_OK;
	}  

	enc->buffer_yuv = gst_buffer_new_and_alloc(yuv_size);
	enc->buffer_cbcr = gst_buffer_new_and_alloc(cbcr_size);

	memcpy(GST_BUFFER_DATA(enc->buffer_yuv), GST_BUFFER_DATA(buffer), yuv_size);

	memcpy(GST_BUFFER_DATA(enc->buffer_cbcr),
		   GST_BUFFER_DATA(buffer) + yuv_size, cbcr_size);

	// Buffers are ready to be read
	pthread_mutex_unlock(&enc->mutex);

	gst_buffer_unref(buffer);
	
	if (!enc->enc_thread)
	{
		/* We'll have to launch the encoder in 
		   a separate thread to keep the pipeline running */
		pthread_create(&enc->enc_thread, NULL, 
				gst_sh_video_launch_encoder_thread, enc);
	}

	return GST_FLOW_OK;
}

static gboolean
gst_sh_video_enc_activate_pull(GstPad  *pad,
					 gboolean active)
{
	GstSHVideoEnc *enc = 
		(GstSHVideoEnc *)(GST_OBJECT_PARENT(pad));

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	if (active) 
	{
		enc->offset = 0;
		return gst_pad_start_task(pad,
				(GstTaskFunction)gst_sh_video_enc_loop, enc);
	} 
	else 
	{
		return gst_pad_stop_task(pad);
	}
}

static void
gst_sh_video_enc_loop(GstSHVideoEnc *enc)
{
	GstFlowReturn ret;
	gint yuv_size, cbcr_size;
	GstBuffer* tmp;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	if (!enc->caps_set)
	{
		gst_sh_video_enc_read_sink_caps(enc);
		gst_sh_video_enc_read_src_caps(enc);
		gst_sh_video_enc_init_encoder(enc);
		if (!gst_caps_is_any(enc->out_caps))
		{
			if (!gst_sh_video_enc_set_src_caps(enc))
			{
				gst_pad_pause_task(enc->sinkpad);
				return;
			}
		}
		enc->caps_set = TRUE;
	}

	/* If buffers are not empty we'll have to 
	   wait until encoder has consumed data */
	if (enc->buffer_yuv && enc->buffer_cbcr)
	{
		pthread_mutex_lock(&enc->cond_mutex);
		pthread_cond_wait(&enc->thread_condition, &enc->cond_mutex);
		pthread_mutex_unlock(&enc->cond_mutex);
	}

	// Lock mutex while handling the buffers
	pthread_mutex_lock(&enc->mutex);
	yuv_size = enc->width * enc->height;
	cbcr_size = enc->width * enc->height / 2;

	ret = gst_pad_pull_range(enc->sinkpad, enc->offset,
			yuv_size, &enc->buffer_yuv);

	if (ret != GST_FLOW_OK) 
	{
		GST_DEBUG_OBJECT(enc, "pull_range failed: %s", gst_flow_get_name(ret));
		gst_pad_pause_task(enc->sinkpad);
		enc->eos = TRUE;
		gst_pad_push_event(enc->srcpad, gst_event_new_eos());
		return;
	}
	else if (GST_BUFFER_SIZE(enc->buffer_yuv) != yuv_size)
	{
		GST_DEBUG_OBJECT(enc, "Not enough data");
		gst_pad_pause_task(enc->sinkpad);
		enc->eos = TRUE;
		gst_pad_push_event(enc->srcpad, gst_event_new_eos());
		return;
	}  

	enc->offset += yuv_size;

	ret = gst_pad_pull_range(enc->sinkpad, enc->offset, cbcr_size, &tmp);

	if (ret != GST_FLOW_OK) 
	{
		GST_DEBUG_OBJECT(enc, "pull_range failed: %s", gst_flow_get_name(ret));
		gst_pad_pause_task(enc->sinkpad);
		enc->eos = TRUE;
		gst_pad_push_event(enc->srcpad, gst_event_new_eos());
		return;
	}  
	else if (GST_BUFFER_SIZE(tmp) != cbcr_size)
	{
		GST_DEBUG_OBJECT(enc, "Not enough data");
		gst_pad_pause_task(enc->sinkpad);
		enc->eos = TRUE;
		gst_pad_push_event(enc->srcpad, gst_event_new_eos());
		return;
	}  

	enc->offset += cbcr_size;

	enc->buffer_cbcr = tmp;

	pthread_mutex_unlock(&enc->mutex);
	
	if (!enc->enc_thread)
	{
		/* We'll have to launch the encoder in 
		   a separate thread to keep the pipeline running */
		pthread_create(&enc->enc_thread, NULL, 
					gst_sh_video_launch_encoder_thread, enc);
	}
}

void *
gst_sh_video_launch_encoder_thread(void *data)
{
	gint ret;
	GstSHVideoEnc *enc = (GstSHVideoEnc *)data;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	ret = shcodecs_encoder_run(enc->encoder);

	GST_DEBUG_OBJECT(enc, "shcodecs_encoder_run returned %d\n", ret);
	GST_DEBUG_OBJECT(enc, "%d frames encoded.", enc->frame_number);

	// We can stop waiting if encoding has ended
	pthread_mutex_lock(&enc->cond_mutex);
	pthread_cond_signal(&enc->thread_condition);
	pthread_mutex_unlock(&enc->cond_mutex);

	// Calling stop task won't do any harm if we are in push mode
	gst_pad_stop_task(enc->sinkpad);
	if(!enc->eos)
	{
		enc->eos = TRUE;
		gst_pad_push_event(enc->srcpad, gst_event_new_eos());
    }

	return NULL;
}

static int 
gst_sh_video_enc_get_input(SHCodecs_Encoder * encoder, void *user_data)
{
	GstSHVideoEnc *enc = (GstSHVideoEnc *)user_data;
	gint ret=0;

	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);

	// Lock mutex while reading the buffer  
	pthread_mutex_lock(&enc->mutex); 

	if (enc->stream_stopped || enc->eos)
	{
		GST_DEBUG_OBJECT(enc, "Encoding stop requested, returning 1");
		ret = 1;
	}
	else if (enc->buffer_yuv && enc->buffer_cbcr)
	{
		ret = shcodecs_encoder_input_provide(encoder, 
					 GST_BUFFER_DATA(enc->buffer_yuv),
					 GST_BUFFER_DATA(enc->buffer_cbcr));

		gst_buffer_unref(enc->buffer_yuv);
		enc->buffer_yuv = NULL;
		gst_buffer_unref(enc->buffer_cbcr);
		enc->buffer_cbcr = NULL;  

		// Signal the main thread that buffers are read
		pthread_mutex_lock(&enc->cond_mutex);
		pthread_cond_signal(&enc->thread_condition);
		pthread_mutex_unlock(&enc->cond_mutex);
	}
	pthread_mutex_unlock(&enc->mutex);

	return ret;
}

static int 
gst_sh_video_enc_write_output(SHCodecs_Encoder * encoder,
			unsigned char *data, int length, void *user_data)
{
	GstSHVideoEnc *enc = (GstSHVideoEnc *)user_data;
	GstBuffer* buf = NULL;
	gint ret = 0;

	GST_LOG_OBJECT(enc, "%s called. Got %d bytes data frame number: %d\n", 
				   __FUNCTION__, length, enc->frame_number);

	pthread_mutex_lock(&enc->mutex); 

	if (enc->stream_stopped)
	{
		GST_DEBUG_OBJECT(enc, "Encoding stop requested, returning 1");
		ret = 1;
	}
	else if (length)
	{
		buf = gst_buffer_new();
		gst_buffer_set_data(buf, data, length);

		GST_BUFFER_DURATION(buf) = 
			enc->fps_denominator * 1000 * GST_MSECOND / enc->fps_numerator;
		GST_BUFFER_TIMESTAMP(buf) = enc->frame_number*GST_BUFFER_DURATION(buf);
		GST_BUFFER_OFFSET(buf) = enc->frame_number; 
		enc->frame_number++;

		if (gst_pad_push(enc->srcpad, buf) != GST_FLOW_OK) 
		{
			GST_DEBUG_OBJECT(enc, "pad_push failed: %s", 
								gst_flow_get_name(ret));
			ret = 1;
		}
	}
	pthread_mutex_unlock(&enc->mutex); 
	return ret;
}

static gboolean
gst_sh_video_enc_src_query(GstPad * pad, GstQuery * query)
{
	GstSHVideoEnc *enc = 
		(GstSHVideoEnc *)(GST_OBJECT_PARENT(pad));
	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);
	return gst_pad_query_default(pad, query);
}

gboolean 
gst_sh_video_enc_set_encoding_properties(GstSHVideoEnc *enc)
{
	GST_LOG_OBJECT(enc, "%s called", __FUNCTION__);
	
	//Stream dependent defaults
    if (enc->format == SHCodecs_Format_H264)
	{
		if (!enc->bitrate)
		{
			enc->bitrate = DEFAULT_BITRATE_H264;
		}
		if (!enc->search_mode)
		{
			enc->search_mode = DEFAULT_SEARCH_MODE_H264;
		}
		if (!enc->i_vop_quant_initial_value)
		{
			enc->i_vop_quant_initial_value = DEFAULT_I_VOP_QUANT_INITIAL_VALUE_H264;
		}
		if (!enc->p_vop_quant_initial_value)
		{
			enc->p_vop_quant_initial_value = DEFAULT_P_VOP_QUANT_INITIAL_VALUE_H264;
		}
		if (!enc->clip_d_quant_frame)
		{
			enc->clip_d_quant_frame = DEFAULT_CLIP_D_QUANT_FRAME_H264;
		}
		if (!enc->quant_min_i_vop_under_range)
		{
			enc->quant_min_i_vop_under_range = DEFAULT_QUANT_MIN_I_VOP_UNDER_RANGE_H264;
		}
		if (!enc->quant_min)
		{
			enc->quant_min = DEFAULT_QUANT_MIN_H264;
		}
		if (!enc->quant_max)
		{
			enc->quant_max = DEFAULT_QUANT_MAX_H264;
		}
	}
	else
	{
		if (!enc->bitrate)
		{
			enc->bitrate = DEFAULT_BITRATE_MPEG4;
		}
		if (!enc->search_mode)
		{
			enc->search_mode = DEFAULT_SEARCH_MODE_MPEG4;
		}
		if (!enc->i_vop_quant_initial_value)
		{
			enc->i_vop_quant_initial_value = DEFAULT_I_VOP_QUANT_INITIAL_VALUE_MPEG4;
		}
		if (!enc->p_vop_quant_initial_value)
		{
			enc->p_vop_quant_initial_value = DEFAULT_P_VOP_QUANT_INITIAL_VALUE_MPEG4;
		}
		if (!enc->clip_d_quant_frame)
		{
			enc->clip_d_quant_frame = DEFAULT_CLIP_D_QUANT_FRAME_MPEG4;
		}
		if (!enc->quant_min_i_vop_under_range)
		{
			enc->quant_min_i_vop_under_range = DEFAULT_QUANT_MIN_I_VOP_UNDER_RANGE_MPEG4;
		}
		if (!enc->quant_min)
		{
			enc->quant_min = DEFAULT_QUANT_MIN_MPEG4;
		}
		if (!enc->quant_max)
		{
			enc->quant_max = DEFAULT_QUANT_MAX_MPEG4;
		}
	}

  	// COMMON
	if (shcodecs_encoder_set_bitrate(enc->encoder, enc->bitrate) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_I_vop_interval(enc->encoder, enc->i_vop_interval) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_mv_mode(enc->encoder, enc->mv_mode) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_fcode_forward(enc->encoder, enc->fcode_forward) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_search_mode(enc->encoder, enc->search_mode) == -1)
	{
		return FALSE;
	}

	if (shcodecs_encoder_set_search_time_fixed(enc->encoder, enc->search_time_fixed) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_ratecontrol_skip_enable(enc->encoder, enc->ratecontrol_skip_enable) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_ratecontrol_use_prevquant(enc->encoder, enc->ratecontrol_use_prevquant) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_ratecontrol_respect_type(enc->encoder, enc->ratecontrol_respect_type) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_ratecontrol_intra_thr_changeable(enc->encoder, enc->ratecontrol_intra_thr_changeable) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_control_bitrate_length(enc->encoder, enc->control_bitrate_length) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_intra_macroblock_refresh_cycle(enc->encoder, enc->intra_macroblock_refresh_cycle) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_video_format(enc->encoder, enc->video_format) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_frame_num_resolution(enc->encoder, enc->frame_num_resolution) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_noise_reduction(enc->encoder, enc->noise_reduction) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_reaction_param_coeff(enc->encoder, enc->reaction_param_coeff) == -1)
	{
		return FALSE;
	}
	if (shcodecs_encoder_set_weightedQ_mode(enc->encoder, enc->weighted_q_mode) == -1)
	{
		return FALSE;
	}

    if (enc->format == SHCodecs_Format_H264)
	{
		if (shcodecs_encoder_set_h264_Ivop_quant_initial_value(enc->encoder, enc->i_vop_quant_initial_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_Pvop_quant_initial_value(enc->encoder, enc->p_vop_quant_initial_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_use_dquant(enc->encoder, enc->use_d_quant) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_clip_dquant_next_mb(enc->encoder, enc->clip_d_quant_next_mb) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_clip_dquant_frame(enc->encoder, enc->clip_d_quant_frame) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_quant_min(enc->encoder, enc->quant_min) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_quant_min_Ivop_under_range(enc->encoder, enc->quant_min_i_vop_under_range) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_quant_max(enc->encoder, enc->quant_max) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_skipcheck_enable(enc->encoder, enc->ratecontrol_cpb_skipcheck_enable) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_Ivop_noskip(enc->encoder, enc->ratecontrol_cpb_i_vop_noskip) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_remain_zero_skip_enable(enc->encoder, enc->ratecontrol_cpb_remain_zero_skip_enable) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_offset(enc->encoder, enc->ratecontrol_cpb_offset) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_offset_rate(enc->encoder, enc->ratecontrol_cpb_offset_rate) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_buffer_mode(enc->encoder, enc->ratecontrol_cpb_buffer_mode) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_max_size(enc->encoder, enc->ratecontrol_cpb_max_size) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_ratecontrol_cpb_buffer_unit_size(enc->encoder, enc->ratecontrol_cpb_buffer_unit_size) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_intra_thr_1(enc->encoder, enc->intra_thr_1) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_intra_thr_2(enc->encoder, enc->intra_thr_2) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_sad_intra_bias(enc->encoder, enc->sad_intra_bias) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_regularly_inserted_I_type(enc->encoder, enc->regularly_inserted_i_type) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_call_unit(enc->encoder, enc->call_unit) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_use_slice(enc->encoder, enc->use_slice) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_slice_size_mb(enc->encoder, enc->slice_size_mb) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_slice_size_bit(enc->encoder, enc->slice_size_bit) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_slice_type_value_pattern(enc->encoder, enc->slice_type_value_pattern) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_use_mb_partition(enc->encoder, enc->use_mb_partition) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_mb_partition_vector_thr(enc->encoder, enc->mb_partition_vector_thr) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_deblocking_mode(enc->encoder, enc->deblocking_mode) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_use_deblocking_filter_control(enc->encoder, enc->use_deblocking_filter_control) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_deblocking_alpha_offset(enc->encoder, enc->deblocking_alpha_offset) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_deblocking_beta_offset(enc->encoder, enc->deblocking_beta_offset) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_me_skip_mode(enc->encoder, enc->me_skip_mode) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_put_start_code(enc->encoder, enc->put_start_code) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_param_changeable(enc->encoder, enc->param_changeable) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_changeable_max_bitrate(enc->encoder, enc->changeable_max_bitrate) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_seq_param_set_id(enc->encoder, enc->seq_param_set_id) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_profile(enc->encoder, enc->profile) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_constraint_set_flag(enc->encoder, enc->constraint_set_flag) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_level_type(enc->encoder, enc->level_type) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_level_value(enc->encoder, enc->level_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_out_vui_parameters(enc->encoder, enc->out_vui_parameters) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_chroma_qp_index_offset(enc->encoder, enc->chroma_qp_index_offset) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_h264_constrained_intra_pred(enc->encoder, enc->constrained_intra_pred) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_ref_frame_num(enc->encoder, enc->ref_frame_num) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_output_filler_enable(enc->encoder, enc->output_filler_enable) == -1)
		{
			return FALSE;
		}
	}
	else
	{
		if (shcodecs_encoder_set_mpeg4_out_vos(enc->encoder, enc->out_vos) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_out_gov(enc->encoder, enc->out_gov) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_aspect_ratio_info_type(enc->encoder, enc->aspect_ratio_info_type) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_aspect_ratio_info_value(enc->encoder, enc->aspect_ratio_info_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_vos_profile_level_type(enc->encoder, enc->vos_profile_level_type) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_vos_profile_level_value(enc->encoder, enc->vos_profile_level_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_out_visual_object_identifier(enc->encoder, enc->out_visual_object_identifier) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_visual_object_verid(enc->encoder, enc->visual_object_verid) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_visual_object_priority(enc->encoder, enc->visual_object_priority) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_video_object_type_indication(enc->encoder, enc->video_object_type_indication) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_out_object_layer_identifier(enc->encoder, enc->out_object_layer_identifier) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_video_object_layer_verid(enc->encoder, enc->video_object_layer_verid) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_video_object_layer_priority(enc->encoder, enc->video_object_layer_priority) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_error_resilience_mode(enc->encoder, enc->error_resilience_mode) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_video_packet_size_mb(enc->encoder, enc->video_packet_size_mb) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_video_packet_size_bit(enc->encoder, enc->video_packet_size_bit) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_video_packet_header_extention(enc->encoder, enc->video_packet_header_extention) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_data_partitioned(enc->encoder, enc->data_partitioned) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_reversible_vlc(enc->encoder, enc->reversible_vlc) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_high_quality(enc->encoder, enc->high_quality) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_param_changeable(enc->encoder, enc->param_changeable) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_changeable_max_bitrate(enc->encoder, enc->changeable_max_bitrate) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_Ivop_quant_initial_value(enc->encoder, enc->i_vop_quant_initial_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_Pvop_quant_initial_value(enc->encoder, enc->p_vop_quant_initial_value) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_use_dquant(enc->encoder, enc->use_d_quant) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_clip_dquant_frame(enc->encoder, enc->clip_d_quant_frame) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_quant_min(enc->encoder, enc->quant_min) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_quant_min_Ivop_under_range(enc->encoder, enc->quant_min_i_vop_under_range) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_quant_max(enc->encoder, enc->quant_max) == -1)
		{
			return FALSE;
		}

		//	THESE WERE COMMENTED OUT IN THE CONTROL FILE UTILS
		//if (shcodecs_encoder_set_mpeg4_ratecontrol_rcperiod_skipcheck_enable(enc->encoder, enc->) == -1)
		//{
		//return FALSE;
		//}
		//if (shcodecs_encoder_set_mpeg4_ratecontrol_rcperiod_Ivop_noskip(enc->encoder, enc->) == -1)
		//{
		//return FALSE;
		//}

		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_skipcheck_enable(enc->encoder, enc->ratecontrol_vbv_skipcheck_enable) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_Ivop_noskip(enc->encoder, enc->ratecontrol_vbv_i_vop_noskip) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_remain_zero_skip_enable(enc->encoder, enc->ratecontrol_vbv_remain_zero_skip_enable) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_buffer_unit_size(enc->encoder, enc->ratecontrol_vbv_buffer_unit_size) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_buffer_mode(enc->encoder, enc->ratecontrol_vbv_buffer_mode) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_max_size(enc->encoder, enc->ratecontrol_vbv_max_size) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_offset(enc->encoder, enc->ratecontrol_vbv_offset) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_ratecontrol_vbv_offset_rate(enc->encoder, enc->ratecontrol_vbv_offset_rate) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_quant_type(enc->encoder, enc->quant_type) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_use_AC_prediction(enc->encoder, enc->use_ac_prediction) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_vop_min_mode(enc->encoder, enc->vop_min_mode) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_vop_min_size(enc->encoder, enc->vop_min_size) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_intra_thr(enc->encoder, enc->intra_thr) == -1)
		{
			return FALSE;
		}
		if (shcodecs_encoder_set_mpeg4_b_vop_num(enc->encoder, enc->b_vop_num) == -1)
		{
			return FALSE;
		}			
	}

	return TRUE;
}
