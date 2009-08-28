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
 * The encoding settings are defined in a control file, which is given to the
 * encoder as a property.
 * 
 * \section enc-examples Example launch lines
 * \code
 * gst-launch filesrc location=test.yuv ! gst-sh-mobile-enc cntl-file=m4v.ctl
 * ! filesink location=test.m4v
 * \endcode
 * Very simple pipeline where filesrc and filesink elements are used for reading
 * the raw data and writing the encoded data. In this pipeline the
 * gst-sh-mobile-enc operates in pull mode, so it is the element driving the data
 * flow.
 *
 * \code
 * gst-launch v4l2src device=/dev/video0 ! image/jpeg,width=320,height=240,
 * framerate=15/1 ! jpegdec ! ffmpegcolorspace ! gst-sh-mobile-enc
 * cntl_file=KMp4_000.ctl ! filesink location=test.m4v
 * \endcode
 * In this example, web cam is used as the streaming source via v4l2src element.
 * the webcam in this example provides jpeg -image data. This pipeline works in
 * push mode, so we need to specify the stream properties using static caps after
 * the v4l2src element. jpegdec decodes the jpeg images and ffmpegcolorspace is
 * used to convert the image data for the encoder. Again, filesink is used to
 * write the encoded video stream to a file.
 *
 * \section enc-properties Properties
 * \copydoc gstshvideoencproperties
 *
 * \section enc-pads Pads
 * \copydoc enc_sink_factory
 * \copydoc enc_src_factory
 * 
 * \section enc-license Licensing
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
#include "cntlfile/ControlFileUtil.h"

/**
 * \var enc_sink_factory
 * Name: sink \n
 * Direction: sink \n
 * Available: always \n
 * Caps:
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[48,720], 
 *   height=(int)[48,576], framerate=(fraction)[1,25]
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[48,720], 
 *   height=(int)[48,480], framerate=(fraction)[1,30]
 */
static GstStaticPadTemplate enc_sink_factory = 
  GST_STATIC_PAD_TEMPLATE ("sink",
			   GST_PAD_SINK,
			   GST_PAD_ALWAYS,
			   GST_STATIC_CAPS (
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
 * - video/mpeg, width=(int)[48,720], height=(int)[48,576], 
 *   framerate=(fraction)[1,25], mpegversion=(int)4
 * - video/mpeg, width=(int)[48,720], height=(int)[48,480], 
 *   framerate=(fraction)[1,30], mpegversion=(int)4
 * - video/x-h264, width=(int)[48,720], height=(int)[48,576], 
 *   framerate=(fraction)[1,25], h264version=(int)h264
 * - video/x-h264, width=(int)[48,720], height=(int)[48,480], 
 *   framerate=(fraction)[1,30], h264version=(int)h264
 */
static GstStaticPadTemplate enc_src_factory = 
  GST_STATIC_PAD_TEMPLATE ("src",
			   GST_PAD_SRC,
			   GST_PAD_ALWAYS,
			   GST_STATIC_CAPS (
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

GST_DEBUG_CATEGORY_STATIC (gst_sh_mobile_debug);
#define GST_CAT_DEFAULT gst_sh_mobile_debug

static GstElementClass *parent_class = NULL;

/**
 * \enum gstshvideoencproperties
 * gst-sh-mobile-enc has following property:
 * - "cntl-file" (string). Name of the control file containing encoding
 *   parameters. Default: NULL
 */
enum gstshvideoencproperties
{
  PROP_0,
  PROP_CNTL_FILE,
  PROP_LAST
};


/** Initialize shvideoenc class plugin event handler
    @param g_class Gclass
    @param data user data pointer, unused in the function
*/

static void gst_shvideo_enc_init_class (gpointer g_class, gpointer data);

/** Initialize SH hardware video encoder
    @param klass Gstreamer element class
*/

static void gst_shvideo_enc_base_init (gpointer klass);
 
/** Dispose encoder
    @param object Gstreamer element class
*/

static void gst_shvideo_enc_dispose (GObject * object);

/** Initialize the class for encoder
    @param klass Gstreamer SH video encoder class
*/

static void gst_shvideo_enc_class_init (GstshvideoEncClass *klass);

/** Initialize the encoder
    @param shvideoenc Gstreamer SH video element
    @param gklass Gstreamer SH video encode class
*/

static void gst_shvideo_enc_init (GstshvideoEnc *shvideoenc,
				  GstshvideoEncClass *gklass);
/** Event handler for encoder sink events
    @param pad Gstreamer sink pad
    @param event The Gstreamer event
    @return returns true if the event can be handled, else false
*/

static gboolean gst_shvideo_enc_sink_event (GstPad * pad, GstEvent * event);

/** Initialize the encoder sink pad 
    @param pad Gstreamer sink pad
    @param caps The capabilities of the data to encode
    @return returns true if the video capatilies are supported and the video can be decoded, else false
*/

static gboolean gst_shvideoenc_setcaps (GstPad * pad, GstCaps * caps);

/** Encoder active event and checks is this pull or push event 
    @param pad Gstreamer sink pad
    @return returns true if the event handled with out errors, else false
*/

static gboolean	gst_shvideo_enc_activate (GstPad *pad);

/** Function to start the pad task
    @param pad Gstreamer sink pad
    @param active true if the task needed to be started or false to stop the task
    @return returns true if the event handled with out errors, else false
*/

static gboolean	gst_shvideo_enc_activate_pull (GstPad *pad, gboolean active);

/** The encoder function and launches the thread if needed
    @param pad Gstreamer sink pad
    @param buffer Buffer where to put back the encoded data
    @return returns GST_FLOW_OK if the function with out errors
*/

static GstFlowReturn gst_shvideo_enc_chain (GstPad *pad, GstBuffer *buffer);

/** The encoder sink pad task
    @param enc Gstreamer SH video encoder
*/

static void gst_shvideo_enc_loop (GstshvideoEnc *enc);

/** The function will set the user defined control file name value for decoder
    @param object The object where to get Gstreamer SH video Encoder object
    @param prop_id The property id
    @param value In this case file name if prop_id is PROP_CNTL_FILE
    @param pspec not used in fuction
*/

static void gst_shvideo_enc_set_property (GObject *object, 
					  guint prop_id, const GValue *value, 
					  GParamSpec * pspec);

/** The function will return the control file name from decoder to value
    @param object The object where to get Gstreamer SH video Encoder object
    @param prop_id The property id
    @param value In this case file name if prop_id is PROP_CNTL_FILE
    @param pspec not used in fuction
*/

static void gst_shvideo_enc_get_property (GObject * object, guint prop_id,
					  GValue * value, GParamSpec * pspec);

/** The encoder sink event handler and calls sink pad push event
    @param pad Gstreamer sink pad
    @param event Event information
    @returns Returns the value of gst_pad_push_event()
*/

static gboolean gst_shvideo_enc_sink_event (GstPad * pad, GstEvent * event);

/** Gstreamer source pad query 
    @param pad Gstreamer source pad
    @param query Gsteamer query
    @returns Returns the value of gst_pad_query_default
*/

static gboolean gst_shvideo_enc_src_query (GstPad * pad, GstQuery * query);

/** Callback function for the encoder input
    @param encoder shcodecs encoder
    @param user_data Gstreamer SH encoder object
    @return 0 if encoder should continue. 1 if encoder should pause.
*/

static int gst_shvideo_enc_get_input(SHCodecs_Encoder * encoder, void *user_data);

/** Callback function for the encoder output
    @param encoder shcodecs encoder
    @param data the encoded video frame
    @param length size the encoded video frame buffer
    @param user_data Gstreamer SH encoder object
    @return 0 if encoder should continue. 1 if encoder should pause.
*/

static int gst_shvideo_enc_write_output(SHCodecs_Encoder * encoder,
					unsigned char *data, int length, void *user_data);

static GstStateChangeReturn
gst_shvideo_enc_change_state (GstElement *element, GstStateChange transition);


static void
gst_shvideo_enc_init_class (gpointer g_class, gpointer data)
{
  parent_class = g_type_class_peek_parent (g_class);
  gst_shvideo_enc_class_init ((GstshvideoEncClass *) g_class);
}

GType gst_shvideo_enc_get_type (void)
{
  static GType object_type = 0;

  if (object_type == 0) {
    static const GTypeInfo object_info = {
      sizeof (GstshvideoEncClass),
      gst_shvideo_enc_base_init,
      NULL,
      gst_shvideo_enc_init_class,
      NULL,
      NULL,
      sizeof (GstshvideoEnc),
      0,
      (GInstanceInitFunc) gst_shvideo_enc_init
    };
    
    object_type =
      g_type_register_static (GST_TYPE_ELEMENT, "gst-sh-mobile-enc", 
			      &object_info, (GTypeFlags) 0);
  }
  
  return object_type;
}

static void
gst_shvideo_enc_base_init (gpointer klass)
{
  static const GstElementDetails plugin_details =
    GST_ELEMENT_DETAILS ("SH hardware video encoder",
			 "Codec/Encoder/Video",
			 "Encode mpeg-based video stream (mpeg4, h264)",
			 "Johannes Lahti <johannes.lahti@nomovok.com>");
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&enc_src_factory));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&enc_sink_factory));
  gst_element_class_set_details (element_class, &plugin_details);
}

static void
gst_shvideo_enc_dispose (GObject * object)
{
  GstshvideoEnc *shvideoenc = GST_SHVIDEOENC (object);

  if (shvideoenc->encoder!=NULL) {
    shcodecs_encoder_close(shvideoenc->encoder);
    shvideoenc->encoder= NULL;
  }

  pthread_mutex_destroy(&shvideoenc->mutex);
  pthread_mutex_destroy(&shvideoenc->cond_mutex);
  pthread_cond_destroy(&shvideoenc->thread_condition);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_shvideo_enc_class_init (GstshvideoEncClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->dispose = gst_shvideo_enc_dispose;
  gobject_class->set_property = gst_shvideo_enc_set_property;
  gobject_class->get_property = gst_shvideo_enc_get_property;

  GST_DEBUG_CATEGORY_INIT (gst_sh_mobile_debug, "gst-sh-mobile-enc",
      0, "Encoder for H264/MPEG4 streams");

  g_object_class_install_property (gobject_class, PROP_CNTL_FILE,
      g_param_spec_string ("cntl-file", "Control file location", 
			"Location of the file including encoding parameters", 
			   NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstelement_class->change_state = gst_shvideo_enc_change_state;
}

static void
gst_shvideo_enc_init (GstshvideoEnc * shvideoenc,
    GstshvideoEncClass * gklass)
{
  GstElementClass *klass = GST_ELEMENT_GET_CLASS (shvideoenc);

  GST_LOG_OBJECT(shvideoenc,"%s called",__FUNCTION__);

  shvideoenc->sinkpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "sink"), "sink");

  gst_element_add_pad (GST_ELEMENT (shvideoenc), shvideoenc->sinkpad);

  gst_pad_set_setcaps_function (shvideoenc->sinkpad, gst_shvideoenc_setcaps);
  gst_pad_set_activate_function (shvideoenc->sinkpad, gst_shvideo_enc_activate);
  gst_pad_set_activatepull_function (shvideoenc->sinkpad,
      gst_shvideo_enc_activate_pull);
  gst_pad_set_event_function(shvideoenc->sinkpad, gst_shvideo_enc_sink_event);
  gst_pad_set_chain_function(shvideoenc->sinkpad, gst_shvideo_enc_chain);
  shvideoenc->srcpad =
      gst_pad_new_from_template (gst_element_class_get_pad_template (klass,
          "src"), "src");
  gst_pad_use_fixed_caps (shvideoenc->srcpad);

  gst_pad_set_query_function (shvideoenc->srcpad,
      GST_DEBUG_FUNCPTR (gst_shvideo_enc_src_query));

  gst_element_add_pad (GST_ELEMENT (shvideoenc), shvideoenc->srcpad);

  shvideoenc->encoder=NULL;
  shvideoenc->caps_set=FALSE;
  shvideoenc->enc_thread = 0;
  shvideoenc->buffer_yuv = NULL;
  shvideoenc->buffer_cbcr = NULL;

  pthread_mutex_init(&shvideoenc->mutex,NULL);
  pthread_mutex_init(&shvideoenc->cond_mutex,NULL);
  pthread_cond_init(&shvideoenc->thread_condition,NULL);

  shvideoenc->format = SHCodecs_Format_NONE;
  shvideoenc->out_caps = NULL;
  shvideoenc->width = 0;
  shvideoenc->height = 0;
  shvideoenc->fps_numerator = 0;
  shvideoenc->fps_denominator = 0;
  shvideoenc->frame_number = 0;

  shvideoenc->stream_paused = FALSE;
}

static void
gst_shvideo_enc_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstshvideoEnc *shvideoenc = GST_SHVIDEOENC (object);
  
  switch (prop_id) 
  {
    case PROP_CNTL_FILE:
    {
      strcpy(shvideoenc->ainfo.ctrl_file_name_buf,g_value_get_string(value));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
  }
}

static void
gst_shvideo_enc_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstshvideoEnc *shvideoenc = GST_SHVIDEOENC (object);

  switch (prop_id) 
  {
    case PROP_CNTL_FILE:
    {
      g_value_set_string(value,shvideoenc->ainfo.ctrl_file_name_buf);      
      break;
    }
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean 
gst_shvideo_enc_sink_event (GstPad * pad, GstEvent * event)
{
  GstshvideoEnc *enc = (GstshvideoEnc *) (GST_OBJECT_PARENT (pad));  

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);

  return gst_pad_push_event(enc->srcpad,event);
}

static gboolean
gst_shvideoenc_setcaps (GstPad * pad, GstCaps * caps)
{
  gboolean ret;
  GstStructure *structure;
  GstshvideoEnc *enc = 
    (GstshvideoEnc *) (GST_OBJECT_PARENT (pad));

  enc->caps_set = FALSE;
  ret = TRUE;

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);

  // get input size
  structure = gst_caps_get_structure (caps, 0);
  ret = gst_structure_get_int (structure, "width", &enc->width);
  ret &= gst_structure_get_int (structure, "height", &enc->height);
  ret &= gst_structure_get_fraction (structure, "framerate", 
				     &enc->fps_numerator, 
				     &enc->fps_denominator);

  if(!ret) {
	return ret;
  }

  gst_shvideoenc_read_src_caps(enc);
  gst_shvideo_enc_init_encoder(enc);

  if(!gst_caps_is_any(enc->out_caps))
  {
    ret = gst_shvideoenc_set_src_caps(enc);
  }
    
  if(ret) {
    enc->caps_set = TRUE;
  }

  return ret;
}

void
gst_shvideoenc_read_src_caps(GstshvideoEnc * shvideoenc)
{
  GstStructure *structure;

  GST_LOG_OBJECT(shvideoenc,"%s called",__FUNCTION__);

  // get the caps of the next element in chain
  shvideoenc->out_caps = gst_pad_peer_get_caps(shvideoenc->srcpad);
  
  // Any format is ok too
  if(!gst_caps_is_any(shvideoenc->out_caps))
  {
    structure = gst_caps_get_structure (shvideoenc->out_caps, 0);
    if (!strcmp (gst_structure_get_name (structure), "video/mpeg")) {
      shvideoenc->format = SHCodecs_Format_MPEG4;
    }
    else if (!strcmp (gst_structure_get_name (structure), "video/x-h264")) {
      shvideoenc->format = SHCodecs_Format_H264;
    }
  }
}

gboolean
gst_shvideoenc_set_src_caps(GstshvideoEnc * shvideoenc)
{
  GstCaps* caps = NULL;
  gboolean ret = TRUE;
  
  GST_LOG_OBJECT(shvideoenc,"%s called",__FUNCTION__);

  if(shvideoenc->format == SHCodecs_Format_MPEG4)
  {
    caps = gst_caps_new_simple ("video/mpeg", "width", G_TYPE_INT, 
				shvideoenc->width, "height", G_TYPE_INT, 
				shvideoenc->height, "framerate", 
				GST_TYPE_FRACTION, shvideoenc->fps_numerator, 
				shvideoenc->fps_denominator, "mpegversion", 
				G_TYPE_INT, 4, NULL);
  }
  else if(shvideoenc->format == SHCodecs_Format_H264)
  {
    caps = gst_caps_new_simple ("video/x-h264", "width", G_TYPE_INT, 
				shvideoenc->width, "height", G_TYPE_INT, 
				shvideoenc->height, "framerate", 
				GST_TYPE_FRACTION, shvideoenc->fps_numerator, 
				shvideoenc->fps_denominator, NULL);
  }
  else
  {
    GST_ELEMENT_ERROR((GstElement*)shvideoenc,CORE,NEGOTIATION,
		      ("Format undefined."), (NULL));
  }

  if(!gst_pad_set_caps(shvideoenc->srcpad,caps))
  {
    GST_ELEMENT_ERROR((GstElement*)shvideoenc,CORE,NEGOTIATION,
		      ("Source pad not linked."), (NULL));
    ret = FALSE;
  }
  if(!gst_pad_set_caps(gst_pad_get_peer(shvideoenc->srcpad),caps))
  {
    GST_ELEMENT_ERROR((GstElement*)shvideoenc,CORE,NEGOTIATION,
		      ("Source pad not linked."), (NULL));
    ret = FALSE;
  }
  gst_caps_unref(caps);
  return ret;
}

void
gst_shvideo_enc_init_encoder(GstshvideoEnc * shvideoenc)
{
  gint ret = 0;
  glong fmt = 0;

  GST_LOG_OBJECT(shvideoenc,"%s called",__FUNCTION__);

  ret = GetFromCtrlFTop((const char *)
				shvideoenc->ainfo.ctrl_file_name_buf,
				&shvideoenc->ainfo,
				&fmt);
  if (ret < 0) {
    GST_ELEMENT_ERROR((GstElement*)shvideoenc,CORE,FAILED,
		      ("Error reading control file."), (NULL));
  }

  if(shvideoenc->format == SHCodecs_Format_NONE)
  {
    shvideoenc->format = fmt;
  }

  if(!shvideoenc->width)
  {
    shvideoenc->width = shvideoenc->ainfo.xpic;
  }

  if(!shvideoenc->height)
  {
    shvideoenc->height = shvideoenc->ainfo.ypic;
  }

  shvideoenc->encoder = shcodecs_encoder_init(shvideoenc->width, 
					      shvideoenc->height, 
					      shvideoenc->format);

  shcodecs_encoder_set_input_callback(shvideoenc->encoder, 
				      gst_shvideo_enc_get_input, 
				      shvideoenc);
  shcodecs_encoder_set_output_callback(shvideoenc->encoder, 
				       gst_shvideo_enc_write_output, 
				       shvideoenc);

  ret =
      GetFromCtrlFtoEncParam(shvideoenc->encoder, &shvideoenc->ainfo);

  if (ret < 0) {
    GST_ELEMENT_ERROR((GstElement*)shvideoenc,CORE,FAILED,
		      ("Error reading control file."), (NULL));
  }

  if(shvideoenc->fps_numerator && shvideoenc->fps_denominator)
  {
    shcodecs_encoder_set_frame_rate(shvideoenc->encoder,
	  (shvideoenc->fps_numerator/shvideoenc->fps_denominator)*10);
  }
  shcodecs_encoder_set_xpic_size(shvideoenc->encoder,shvideoenc->width);
  shcodecs_encoder_set_ypic_size(shvideoenc->encoder,shvideoenc->height);

  shcodecs_encoder_set_frame_no_increment(shvideoenc->encoder,1); 
  shcodecs_encoder_set_frame_number_to_encode(shvideoenc->encoder,-1); 

  GST_DEBUG_OBJECT(shvideoenc,"Encoder init: %ldx%ld %ldfps format:%ld",
		   shcodecs_encoder_get_xpic_size(shvideoenc->encoder),
		   shcodecs_encoder_get_ypic_size(shvideoenc->encoder),
		   shcodecs_encoder_get_frame_rate(shvideoenc->encoder)/10,
		   shcodecs_encoder_get_stream_type(shvideoenc->encoder)); 
}

static gboolean
gst_shvideo_enc_activate (GstPad * pad)
{
  gboolean ret;
  GstshvideoEnc *enc = 
    (GstshvideoEnc *) (GST_OBJECT_PARENT (pad));

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);
  if (gst_pad_check_pull_range (pad)) {
    GST_LOG_OBJECT(enc,"PULL mode");
    ret = gst_pad_activate_pull (pad, TRUE);
  } else {  
    GST_LOG_OBJECT(enc,"PUSH mode");
    ret = gst_pad_activate_push (pad, TRUE);
  }
  return ret;
}

static GstStateChangeReturn
gst_shvideo_enc_change_state (GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstshvideoEnc *enc = GST_SHVIDEOENC (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      {
        GST_DEBUG_OBJECT(enc,"Stopping encoding");
        pthread_mutex_lock( &enc->mutex );
        enc->stream_paused = TRUE;        
        pthread_mutex_unlock( &enc->mutex );
	//This doesn't work until shcodecs implements returning
	//from encoding when input/output callback returns 1
        //pthread_join(enc->enc_thread,NULL);
        //GST_DEBUG_OBJECT(enc,"Encoder thread returned");
        pthread_cancel(enc->enc_thread);
        break;
      }
    default:
      break;
  }

  return ret;
}

static GstFlowReturn 
gst_shvideo_enc_chain (GstPad * pad, GstBuffer * buffer)
{
  gint yuv_size, cbcr_size, i, j;
  guint8* cr_ptr; 
  guint8* cb_ptr;
  GstshvideoEnc *enc = (GstshvideoEnc *) (GST_OBJECT_PARENT (pad));  

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);

  if(enc->stream_paused)
  {
    return GST_FLOW_UNEXPECTED;
  }

  if(!enc->caps_set)
  {
    gst_shvideoenc_read_src_caps(enc);
    gst_shvideo_enc_init_encoder(enc);
    if(!gst_caps_is_any(enc->out_caps))
    {
      if(!gst_shvideoenc_set_src_caps(enc))
      {
	return GST_FLOW_UNEXPECTED;
      }
    }
    enc->caps_set = TRUE;
  }

  /* If buffers are not empty we'll have to 
     wait until encoder has consumed data */
  if(enc->buffer_yuv && enc->buffer_cbcr)
  {
    pthread_mutex_lock( &enc->cond_mutex );
    pthread_cond_wait( &enc->thread_condition, &enc->cond_mutex );
    pthread_mutex_unlock( &enc->cond_mutex );
  }

  // Lock mutex while handling the buffers
  pthread_mutex_lock(&enc->mutex);
  yuv_size = enc->width*enc->height;
  cbcr_size = enc->width*enc->height/2;

  // Check that we have got enough data
  if(GST_BUFFER_SIZE(buffer) != yuv_size + cbcr_size)
  {
    GST_DEBUG_OBJECT (enc, "Not enough data");
    // If we can't continue we can issue EOS
    gst_pad_push_event(enc->srcpad,gst_event_new_eos ());
    return GST_FLOW_OK;
  }  

  enc->buffer_yuv = gst_buffer_new_and_alloc (yuv_size);
  enc->buffer_cbcr = gst_buffer_new_and_alloc (cbcr_size);

  memcpy(GST_BUFFER_DATA(enc->buffer_yuv),GST_BUFFER_DATA(buffer),yuv_size);

  if(enc->ainfo.yuv_CbCr_format == 0)
  {
    cb_ptr = GST_BUFFER_DATA(buffer)+yuv_size;
    cr_ptr = GST_BUFFER_DATA(buffer)+yuv_size+(cbcr_size/2);

    for(i=0,j=0;i<cbcr_size;i+=2,j++)
    {
      GST_BUFFER_DATA(enc->buffer_cbcr)[i]=cb_ptr[j];
      GST_BUFFER_DATA(enc->buffer_cbcr)[i+1]=cr_ptr[j];
    }
  }
  else
  {
    memcpy(GST_BUFFER_DATA(enc->buffer_cbcr),GST_BUFFER_DATA(buffer)+yuv_size,
	 cbcr_size);
  }

  // Buffers are ready to be read
  pthread_mutex_unlock(&enc->mutex);

  gst_buffer_unref(buffer);
  
  if(!enc->enc_thread)
  {
    /* We'll have to launch the encoder in 
       a separate thread to keep the pipeline running */
    pthread_create( &enc->enc_thread, NULL, launch_encoder_thread, enc);
  }

  return GST_FLOW_OK;
}

static gboolean
gst_shvideo_enc_activate_pull (GstPad  *pad,
			     gboolean active)
{
  GstshvideoEnc *enc = 
    (GstshvideoEnc *) (GST_OBJECT_PARENT (pad));

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);

  if (active) {
    enc->offset = 0;
    return gst_pad_start_task (pad,
        (GstTaskFunction) gst_shvideo_enc_loop, enc);
  } else {
    return gst_pad_stop_task (pad);
  }
}

static void
gst_shvideo_enc_loop (GstshvideoEnc *enc)
{
  GstFlowReturn ret;
  gint yuv_size, cbcr_size, i, j;
  guint8* cb_ptr; 
  guint8* cr_ptr; 
  GstBuffer* tmp;

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);

  if(!enc->caps_set)
  {
    gst_shvideoenc_read_src_caps(enc);
    gst_shvideo_enc_init_encoder(enc);
    if(!gst_caps_is_any(enc->out_caps))
    {
      if(!gst_shvideoenc_set_src_caps(enc))
      {
	gst_pad_pause_task (enc->sinkpad);
	return;
      }
    }
    enc->caps_set = TRUE;
  }

  /* If buffers are not empty we'll have to 
     wait until encoder has consumed data */
  if(enc->buffer_yuv && enc->buffer_cbcr)
  {
    pthread_mutex_lock( &enc->cond_mutex );
    pthread_cond_wait( &enc->thread_condition, &enc->cond_mutex );
    pthread_mutex_unlock( &enc->cond_mutex );
  }

  // Lock mutex while handling the buffers
  pthread_mutex_lock(&enc->mutex);
  yuv_size = enc->width*enc->height;
  cbcr_size = enc->width*enc->height/2;

  ret = gst_pad_pull_range (enc->sinkpad, enc->offset,
      yuv_size, &enc->buffer_yuv);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (enc, "pull_range failed: %s", gst_flow_get_name (ret));
    gst_pad_pause_task (enc->sinkpad);
    return;
  }
  else if(GST_BUFFER_SIZE(enc->buffer_yuv) != yuv_size)
  {
    GST_DEBUG_OBJECT (enc, "Not enough data");
    gst_pad_pause_task (enc->sinkpad);
    gst_pad_push_event(enc->srcpad,gst_event_new_eos ());
    return;
  }  

  enc->offset += yuv_size;

  ret = gst_pad_pull_range (enc->sinkpad, enc->offset,
      cbcr_size, &tmp);

  if (ret != GST_FLOW_OK) {
    GST_DEBUG_OBJECT (enc, "pull_range failed: %s", gst_flow_get_name (ret));
    gst_pad_pause_task (enc->sinkpad);
    return;
  }  
  else if(GST_BUFFER_SIZE(tmp) != cbcr_size)
  {
    GST_DEBUG_OBJECT (enc, "Not enough data");
    gst_pad_pause_task (enc->sinkpad);
    gst_pad_push_event(enc->srcpad,gst_event_new_eos ());
    return;
  }  

  enc->offset += cbcr_size;

  if(enc->ainfo.yuv_CbCr_format == 0)
  {
    cb_ptr = GST_BUFFER_DATA(tmp);
    cr_ptr = GST_BUFFER_DATA(tmp)+(cbcr_size/2);
    enc->buffer_cbcr = gst_buffer_new_and_alloc(cbcr_size);

    for(i=0,j=0;i<cbcr_size;i+=2,j++)
    {
      GST_BUFFER_DATA(enc->buffer_cbcr)[i]=cb_ptr[j];
      GST_BUFFER_DATA(enc->buffer_cbcr)[i+1]=cr_ptr[j];
    }
    
    gst_buffer_unref(tmp);
  }
  else
  {
    enc->buffer_cbcr = tmp;
  }

  pthread_mutex_unlock(&enc->mutex);
  
  if(!enc->enc_thread)
  {
    /* We'll have to launch the encoder in 
       a separate thread to keep the pipeline running */
    pthread_create( &enc->enc_thread, NULL, launch_encoder_thread, enc);
  }
}

void *
launch_encoder_thread(void *data)
{
  gint ret;
  GstshvideoEnc *enc = (GstshvideoEnc *)data;

  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);

  ret = shcodecs_encoder_run(enc->encoder);

  GST_DEBUG_OBJECT (enc,"shcodecs_encoder_run returned %d\n",ret);

  // We can stop waiting if encoding has ended
  pthread_mutex_lock( &enc->cond_mutex );
  pthread_cond_signal( &enc->thread_condition);
  pthread_mutex_unlock( &enc->cond_mutex );

  // Calling stop task won't do any harm if we are in push mode
  gst_pad_stop_task (enc->sinkpad);
  gst_pad_push_event(enc->srcpad,gst_event_new_eos ());

  return NULL;
}

static int 
gst_shvideo_enc_get_input(SHCodecs_Encoder * encoder, void *user_data)
{
  GstshvideoEnc *shvideoenc = (GstshvideoEnc *)user_data;
  gint ret=0;

  GST_LOG_OBJECT(shvideoenc,"%s called",__FUNCTION__);

  // Lock mutex while reading the buffer  
  pthread_mutex_lock(&shvideoenc->mutex); 
  if ( shvideoenc->stream_paused )
  {
    GST_DEBUG_OBJECT(shvideoenc,"Encoding stop requested, returning 1");
    ret = 1;
  }
  else if(shvideoenc->buffer_yuv && shvideoenc->buffer_cbcr)
  {
    ret = shcodecs_encoder_input_provide(encoder, 
					 GST_BUFFER_DATA(shvideoenc->buffer_yuv),
					 GST_BUFFER_DATA(shvideoenc->buffer_cbcr));

    gst_buffer_unref(shvideoenc->buffer_yuv);
    shvideoenc->buffer_yuv = NULL;
    gst_buffer_unref(shvideoenc->buffer_cbcr);
    shvideoenc->buffer_cbcr = NULL;  

    // Signal the main thread that buffers are read
    pthread_mutex_lock( &shvideoenc->cond_mutex );
    pthread_cond_signal( &shvideoenc->thread_condition);
    pthread_mutex_unlock( &shvideoenc->cond_mutex );
  }
  pthread_mutex_unlock(&shvideoenc->mutex);

  return ret;
}

static int 
gst_shvideo_enc_write_output(SHCodecs_Encoder * encoder,
			unsigned char *data, int length, void *user_data)
{
  GstshvideoEnc *enc = (GstshvideoEnc *)user_data;
  GstBuffer* buf=NULL;
  gint ret=0;

  GST_LOG_OBJECT(enc,"%s called. Got %d bytes data\n",__FUNCTION__, length);

  pthread_mutex_lock(&enc->mutex); 
  if(enc->stream_paused)
  {
    GST_DEBUG_OBJECT(enc,"Encoding stop requested, returning 1");
    ret = 1;
  }
  else if(length)
  {
    buf = gst_buffer_new();
    gst_buffer_set_data(buf, data, length);

    GST_BUFFER_DURATION(buf) = enc->fps_denominator*1000*GST_MSECOND/enc->fps_numerator;
    GST_BUFFER_TIMESTAMP(buf) = enc->frame_number*GST_BUFFER_DURATION(buf);
    enc->frame_number++;

    ret = gst_pad_push (enc->srcpad, buf);

    if (ret != GST_FLOW_OK) {
      GST_DEBUG_OBJECT (enc, "pad_push failed: %s", gst_flow_get_name (ret));
      ret = 1;
    }
  }
  pthread_mutex_unlock(&enc->mutex); 
  return ret;
}

static gboolean
gst_shvideo_enc_src_query (GstPad * pad, GstQuery * query)
{
  GstshvideoEnc *enc = 
    (GstshvideoEnc *) (GST_OBJECT_PARENT (pad));
  GST_LOG_OBJECT(enc,"%s called",__FUNCTION__);
  return gst_pad_query_default (pad, query);
}
