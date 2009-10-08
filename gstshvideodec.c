/**
 * \page dec gst-sh-mobile-dec
 * gst-sh-mobile-dec - Decodes MPEG4/H264 video stream to raw YUV image data 
 * on SuperH environment using libshcodecs HW codec.
 *
 * \section dec-description Description
 * This element is designed to use the HW video processing modules of the
 * Renesas SuperH chipset to decode MPEG4/H264 video streams. This element
 * is not usable in any other environments and it requires libshcodes HW codec
 * to be installed.
 *
 * \section dec-examples Example launch lines
 * 
 * \subsection dec-example-1 Decoding from a file to a file
 * 
 * \code
 * gst-launch filesrc location=test.m4v ! video/mpeg, width=320, height=240,
 * framerate=30/1, mpegversion=4 ! gst-sh-mobile-dec ! filesink location=test.raw
 * \endcode
 * In this pipeline we use filesrc element to read the source file, 
 * which is a MPEG4 video elementary stream. After filesrc we add static caps 
 * as the filesrc does not do caps negotiation and the decoder requires them. 
 * The last element in the pipeline is filesink, which writes the output YUV-data
 * into a file.
 *
 * \subsection dec-example-2 Decoding an AVI -video with audio&video playback
 *
 * \code
 * gst-launch filesrc location=test.avi ! avidemux name=demux demux.audio_00 ! 
 * queue ! decodebin ! audioconvert ! audioresample ! autoaudiosink 
 * demux.video_00 ! queue ! gst-sh-mobile-dec ! gst-sh-mobile-sink
 * \endcode
 *
 * \image html decoder_example.jpeg
 *
 * Filesrc element is used to read the file again, which this time is an AVI
 * wrapped video containing both audio and video stream. avidemux element is 
 * used to strip the avi container. avidemux has two src-pads, which are 
 * named “demux” using a property. Both of the avidemux src pads are first
 * connected to queue elements, which take care of the buffering of the data in
 * the pipeline.
 *  
 * The audio stream is then connected to the decodebin element, which detects
 * the stream format and does the decoding. audioconvert and audioresample 
 * elements are used to transform the data into a suitable format for 
 * playback. The last element in the audio pipeline is autoaudiosink, which
 * automatically detects and connects the correct audio sink for playback. This
 * audio pipeline composition is very common in the gstreamer programming.
 *
 * The video pipeline is constructed from SuperH specific elements; 
 * gst-sh-mobile-dec and gst-sh-mobile-sink. The gst-sh-mobile-sink is a 
 * videosink element for SuperH.
 *
 * \subsection dec-example-3 Decoding a video stream from net
 *  
 * \code
 * gst-launch udpsrc port=5000 caps="application/x-rtp,clock-rate=90000"
 * ! gstrtpjitterbuffer latency=0 ! rtpmp4vdepay ! video/mpeg,width=320,
 * height=240,framerate=15/1 ! gst-sh-mobile-dec ! gst-sh-mobile-sink 
 * \endcode
 * Here the video stream is received from udpsrc element. gstrtpjitterbuffer
 * element is used to take care of ordering and storing the received RTP
 * packages. Next rtpmp4vdepay element is used to remove the RTP frames from
 * the buffers. Again, the static caps are needed to pass information to the 
 * decoder.
 *
 * \section dec-properties Properties
 * \copydoc gstshvideodecproperties
 *
 * \section dec-pads Pads
 * \copydoc dec_sink_factory
 * \copydoc dec_src_factory
 * 
 * \section dec-license License
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
 * Pablo Virolainen <pablo.virolainen@nomovok.com>
 * Johannes Lahti <johannes.lahti@nomovok.com>
 * Aki Honkasuo <aki.honkasuo@nomovok.com>
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <pthread.h>

#include "gstshvideodec.h"
#include "gstshvideosink.h"
#include "gstshvideobuffer.h"

/**
 * \var dec_sink_factory
 * Name: sink \n
 * Direction: sink \n
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
 * - video/x-divx, width=(int)[48,720], height=(int)[48,576], 
 *   framerate=(fraction)[1,25], divxversion=(int){4,5,6}
 * - video/x-divx, width=(int)[48,720], height=(int)[48,480], 
 *   framerate=(fraction)[1,30], divxversion=(int){4,5,6}
 * - video/x-xvid, width=(int)[48,720], height=(int)[48,576], 
 *   framerate=(fraction)[1,25]
 * - video/x-xvid, width=(int)[48,720], height=(int)[48,480], 
 *   framerate=(fraction)[1,30]
 *
 */
static GstStaticPadTemplate dec_sink_factory = 
	GST_STATIC_PAD_TEMPLATE ("sink",
				 GST_PAD_SINK,
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
						  ";"
						  "video/x-h264,"
						  "width  = (int) [48, 720],"
						  "height = (int) [48, 576],"
						  "framerate = (fraction) [0, 25],"
						  "variant = (string) itu,"
						  "h264version = (string) h264"
						  ";"
						  "video/x-h264,"
						  "width  = (int) [48, 720],"
						  "height = (int) [48, 480],"
						  "framerate = (fraction) [0, 30],"
						  "variant = (string) itu,"
						  "h264version = (string) h264"
						  ";"
						  "video/x-divx,"
						  "width  = (int) [48, 720],"
						  "height = (int) [48, 576],"
						  "framerate = (fraction) [0, 25],"
						  "divxversion =  {4, 5, 6}"
						  ";"
						  "video/x-divx,"
						  "width  = (int) [48, 720],"
						  "height = (int) [48, 480],"
						  "framerate = (fraction) [0, 30],"
						  "divxversion =  {4, 5, 6}"
						  ";"
						  "video/x-xvid,"
						  "width  = (int) [48, 720],"
						  "height = (int) [48, 576],"
						  "framerate = (fraction) [0, 25]"
						  ";"
						  "video/x-xvid,"
						  "width  = (int) [48, 720],"
						  "height = (int) [48, 480],"
						  "framerate = (fraction) [0, 30]"
						  )
				 );

/**
 * \var dec_src_factory
 * Name: src \n
 * Direction: src \n
 * Available: always \n
 * Caps:
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[48,720], 
 *   height=(int)[48,576], framerate=(fraction)[1,25]
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[48,720], 
 *   height=(int)[48,480], framerate=(fraction)[1,30]
 */
static GstStaticPadTemplate dec_src_factory = 
	GST_STATIC_PAD_TEMPLATE ("src",
				 GST_PAD_SRC,
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


static GstElementClass *parent_class = NULL;

GST_DEBUG_CATEGORY_STATIC (gst_sh_mobile_debug);
#define GST_CAT_DEFAULT gst_sh_mobile_debug

/**
 * \enum gstshvideodecproperties
 * gst-sh-mobile-dec has following properties:
 * - "buffer-size" (gint). Maximum size of the cache buffer in KB. Default: 1000
 * - "hw-buffer" (string). Enables/disables usage of hardware data buffering.
 *   HW buffering makes zero copy functionality possible if gst-sh-mobile-sink
 *   element is connected to the src -pad. Possible values: "yes"/"no"/"auto". 
 *   Default: auto
 */
enum gstshvideodecproperties
{
	PROP_0,
	PROP_MAX_BUFFER_SIZE,
	PROP_HW_BUFFER,
	PROP_LAST
};

#define DEFAULT_MAX_SIZE 1000 * 1024

#define HW_BUFFER_AUTO "auto"
#define HW_BUFFER_YES  "yes"
#define HW_BUFFER_NO   "no"

enum
{
	HW_ADDR_AUTO,
	HW_ADDR_YES,
	HW_ADDR_NO
};

// STATIC DECLARATIONS

/** 
 * Initialize shvideodec class plugin event handler
 * @param g_class Gclass
 * @param data user data pointer, unused in the function
 */
static void gst_sh_video_dec_init_class (gpointer g_class, gpointer data);

/** 
 * Initialize SH hardware video decoder & sink
 * @param klass Gstreamer element class
 */
static void gst_sh_video_dec_base_init (gpointer klass);

/** 
 * Dispose decoder
 * @param object Gstreamer element class
 */
static void gst_sh_video_dec_dispose (GObject * object);

/** 
 * Initialize the class for decoder and player
 * @param klass Gstreamer SH video decodes class
 */
static void gst_sh_video_dec_class_init (GstSHVideoDecClass * klass);

/** 
 * Initialize the decoder
 * @param dec Gstreamer SH video element
 * @param gklass Gstreamer SH video decode class
 */
static void gst_sh_video_dec_init (GstSHVideoDec * dec, GstSHVideoDecClass * gklass);


/** 
 * The function will set the user defined maximum buffer size value for decoder
 * @param object The object where to get Gstreamer SH video Decoder object
 * @param prop_id The property id
 * @param value In this case maximum buffer size in kilo bytes
 * @param pspec not used in fuction
 */
static void gst_sh_video_dec_set_property (GObject *object, 
					 guint prop_id, const GValue *value, 
					 GParamSpec * pspec);

/** 
 * The function will return the maximum buffer size in kilo bytes from decoder to value
 * @param object The object where to get Gstreamer SH video Decoder object
 * @param prop_id The property id
 * @param value In this case maximum buffer size in kilo bytes
 * @param pspec not used in fuction
 */
static void gst_sh_video_dec_get_property (GObject * object, guint prop_id,
					 GValue * value, GParamSpec * pspec);

/** 
 * Event handler for decoder sink events
 * @param pad Gstreamer sink pad
 * @param event The Gstreamer event
 * @return returns true if the event can be handled, else false
 */
static gboolean gst_sh_video_dec_sink_event (GstPad * pad, GstEvent * event);

/** 
 * Initialize the decoder sink pad 
 * @param pad Gstreamer sink pad
 * @param caps The capabilities of the video to decode
 * @return returns true if the video capatilies are supported and the video can be decoded, else false
 */
static gboolean gst_sh_video_dec_setcaps (GstPad * pad, GstCaps * caps);

/** 
 * GStreamer buffer handling function
 * @param pad Gstreamer sink pad
 * @param inbuffer The input buffer
 * @return returns GST_FLOW_OK if buffer handling was successful. Otherwise GST_FLOW_UNEXPECTED
 */
static GstFlowReturn gst_sh_video_dec_chain (GstPad * pad, GstBuffer * inbuffer);

/** 
 * Event handler for the video frame is decoded and can be shown on screen
 * @param decoder SHCodecs Decoder, unused in the function
 * @param y_buf Userland address to the Y buffer
 * @param y_size Size of the Y buffer
 * @param c_buf Userland address to teh C buffer
 * @param c_size Size of the C buffer
 * @param user_data Contains GstSHVideoDec
 * @return The result of passing data to a pad
 */
static gint gst_shcodecs_decoded_callback (SHCodecs_Decoder * decoder,
					  guchar * y_buf, gint y_size,
					  guchar * c_buf, gint c_size,
					  void * user_data);


// DEFINITIONS

static void
gst_sh_video_dec_init_class (gpointer g_class, gpointer data)
{
	parent_class = g_type_class_peek_parent (g_class);
	gst_sh_video_dec_class_init ((GstSHVideoDecClass *) g_class);
}

GType
gst_sh_video_dec_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) 
	{
		static const GTypeInfo object_info = 
		{
			sizeof (GstSHVideoDecClass),
			gst_sh_video_dec_base_init,
			NULL,
			gst_sh_video_dec_init_class,
			NULL,
			NULL,
			sizeof (GstSHVideoDec),
			0,
			(GInstanceInitFunc) gst_sh_video_dec_init
		};

		object_type = g_type_register_static (GST_TYPE_ELEMENT, 
						      "gst-sh-mobile-dec", 
						      &object_info,
						      (GTypeFlags) 0);
	}
	return object_type;
}

static void
gst_sh_video_dec_base_init (gpointer klass)
{
	static const GstElementDetails plugin_details =
		GST_ELEMENT_DETAILS ("SH hardware video decoder",
				     "Codec/Decoder/Video",
				     "Decode video (H264 && Mpeg4)",
				     "Johannes Lahti <johannes.lahti@nomovok.com>"
				     );

	GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

	gst_element_class_add_pad_template (element_class,
			gst_static_pad_template_get (&dec_sink_factory));
	gst_element_class_add_pad_template (element_class,
			gst_static_pad_template_get (&dec_src_factory));
	gst_element_class_set_details (element_class, &plugin_details);
}

static void
gst_sh_video_dec_dispose (GObject * object)
{
	GstSHVideoDec *dec = GST_SH_VIDEO_DEC (object);

	GST_LOG_OBJECT(dec,"%s called\n",__FUNCTION__);  

	if (dec->decoder != NULL) 
	{
		GST_LOG_OBJECT (dec, "close decoder object %p", dec->decoder);
		shcodecs_decoder_close (dec->decoder);
	}
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_sh_video_dec_class_init (GstSHVideoDecClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	GST_DEBUG_CATEGORY_INIT (gst_sh_mobile_debug, "gst-sh-mobile-dec",
				 0, "Decoder for H264/MPEG4 streams");

	gobject_class->dispose = gst_sh_video_dec_dispose;
	gobject_class->set_property = gst_sh_video_dec_set_property;
	gobject_class->get_property = gst_sh_video_dec_get_property;

	g_object_class_install_property (gobject_class, PROP_MAX_BUFFER_SIZE,
					 g_param_spec_uint ("buffer-size", 
							    "Maximum buffer size kB", 
							    "Maximum size of the cache buffer (kB, 0=disabled)", 
							    0, G_MAXUINT, DEFAULT_MAX_SIZE,
							    G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_HW_BUFFER,
					 g_param_spec_string ("hw-buffer", 
							      "HW-buffer", 
							      "Sets usage of HW buffers (auto(default)/yes/no)",
							      NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_sh_video_dec_init (GstSHVideoDec * dec, GstSHVideoDecClass * gklass)
{
	GstElementClass *kclass = GST_ELEMENT_GET_CLASS (dec);

	GST_LOG_OBJECT(dec,"%s called",__FUNCTION__);

	dec->sinkpad = gst_pad_new_from_template(gst_element_class_get_pad_template(kclass,"sink"),"sink");
	gst_pad_set_setcaps_function(dec->sinkpad, gst_sh_video_dec_setcaps);
	gst_pad_set_chain_function(dec->sinkpad,GST_DEBUG_FUNCPTR(gst_sh_video_dec_chain));
	gst_pad_set_event_function (dec->sinkpad,
				    GST_DEBUG_FUNCPTR (gst_sh_video_dec_sink_event));
	gst_element_add_pad(GST_ELEMENT(dec),dec->sinkpad);

	dec->srcpad = gst_pad_new_from_template(gst_element_class_get_pad_template(kclass,"src"),"src");
	gst_element_add_pad(GST_ELEMENT(dec),dec->srcpad);
	gst_pad_use_fixed_caps (dec->srcpad);

	dec->caps_set = FALSE;
	dec->decoder = NULL;
	dec->running = FALSE;
	dec->use_physical = HW_ADDR_AUTO;

	dec->buffer = NULL;
	dec->buffer_size = DEFAULT_MAX_SIZE;

	pthread_mutex_init(&dec->mutex,NULL);
	pthread_mutex_init(&dec->cond_mutex,NULL);
	pthread_cond_init(&dec->thread_condition,NULL);
}


static void
gst_sh_video_dec_set_property (GObject * object, guint prop_id,
			     const GValue * value, GParamSpec * pspec)
{
	GstSHVideoDec *dec = GST_SH_VIDEO_DEC (object);
	const gchar* string;
	
	GST_LOG_OBJECT(dec,"%s called",__FUNCTION__);

	switch (prop_id) 
	{
		case PROP_MAX_BUFFER_SIZE:
		{
			dec->buffer_size = g_value_get_uint (value) * 1024;
			break;
		}
		case PROP_HW_BUFFER:
		{
			string = g_value_get_string (value);

			if(!strcmp(string,HW_BUFFER_AUTO))
			{
				dec->use_physical = HW_ADDR_AUTO;
			}
			else if(!strcmp(string,HW_BUFFER_YES))
			{
				dec->use_physical = HW_ADDR_YES;
			}
			else if(!strcmp(string,HW_BUFFER_NO))
			{
				dec->use_physical = HW_ADDR_NO; 
			}
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
gst_sh_video_dec_get_property (GObject * object, guint prop_id,
			     GValue * value, GParamSpec * pspec)
{
	GstSHVideoDec *dec = GST_SH_VIDEO_DEC (object);

	GST_DEBUG_OBJECT(dec,"%s called",__FUNCTION__);

	switch (prop_id) 
	{
		case PROP_MAX_BUFFER_SIZE:
		{
			g_value_set_uint(value,dec->buffer_size/1024);      
			break;
		}
		case PROP_HW_BUFFER:
		{
			switch (dec->use_physical)
			{      
				case HW_ADDR_AUTO:
				{
				  g_value_set_string(value, HW_BUFFER_AUTO);
				  break;
				}
				case HW_ADDR_NO:
				{
				  g_value_set_string(value, HW_BUFFER_NO);
				  break;
				}
				case HW_ADDR_YES:
				{
				  g_value_set_string(value, HW_BUFFER_YES);
				  break;
				}
			}
			break;
		}
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static gboolean
gst_sh_video_dec_sink_event (GstPad * pad, GstEvent * event)
{
	GstSHVideoDec *dec = (GstSHVideoDec *) (GST_OBJECT_PARENT (pad));

	GST_DEBUG_OBJECT(dec,"%s called event %i",__FUNCTION__,GST_EVENT_TYPE(event));

	if (GST_EVENT_TYPE (event) == GST_EVENT_EOS) 
	{
		GST_DEBUG_OBJECT (dec, "EOS gst event");
		dec->running = FALSE;
		if(dec->dec_thread)
		{
			pthread_join(dec->dec_thread,NULL);
			// Decode the rest of the buffer
			gst_sh_video_dec_decode(dec);
		}
	}

	return gst_pad_push_event(dec->srcpad,event);
}

static gboolean
gst_sh_video_dec_setcaps (GstPad * pad, GstCaps * sink_caps)
{
	GstStructure *structure = NULL;
	GstCaps* src_caps = NULL;
	GstSHVideoDec *dec = (GstSHVideoDec *) (GST_OBJECT_PARENT (pad));
	gboolean ret = TRUE;

	GST_LOG_OBJECT(dec,"%s called",__FUNCTION__);

	if (dec->decoder!=NULL) 
	{
		GST_DEBUG_OBJECT(dec,"%s: Decoder already opened",__FUNCTION__);
		return FALSE;
	}

	structure = gst_caps_get_structure (sink_caps, 0);

	if (!strcmp (gst_structure_get_name (structure), "video/x-h264")) 
	{
		GST_INFO_OBJECT(dec, "codec format is video/x-h264");
		dec->format = SHCodecs_Format_H264;
	}
	else
	{
		if (!strcmp (gst_structure_get_name (structure), "video/x-divx") ||
		    !strcmp (gst_structure_get_name (structure), "video/x-xvid") ||
		    !strcmp (gst_structure_get_name (structure), "video/mpeg")
		    ) 
		{
			GST_INFO_OBJECT (dec, "codec format is video/mpeg");
			dec->format = SHCodecs_Format_MPEG4;
		} 
		else 
		{
			GST_INFO_OBJECT(dec,"%s failed (not supported: %s)",
					__FUNCTION__,
					gst_structure_get_name (structure));
			return FALSE;
		}
	}

	if(gst_structure_get_fraction (structure, "framerate", 
					&dec->fps_numerator, 
					&dec->fps_denominator))
	{    
		GST_INFO_OBJECT(dec,"Framerate: %d/%d",dec->fps_numerator,
				dec->fps_denominator);
	}
	else
	{
		GST_INFO_OBJECT(dec,"%s failed (no framerate)",__FUNCTION__);
		return FALSE;
	}

	if (gst_structure_get_int (structure, "width",  &dec->width)
	    && gst_structure_get_int (structure, "height", &dec->height))
	{
		GST_INFO_OBJECT(dec,"%s initializing decoder %dx%d",__FUNCTION__,
				dec->width,dec->height);
		dec->decoder=shcodecs_decoder_init(dec->width,dec->height,
						   dec->format);
	} 
	else 
	{
		GST_INFO_OBJECT(dec,"%s failed (no width/height)",__FUNCTION__);
		return FALSE;
	}

	if (dec->decoder==NULL) 
	{
		GST_ELEMENT_ERROR((GstElement*)dec,CORE,FAILED,
				  ("Error on shdecodecs_decoder_init."), 
				  ("%s failed (Error on shdecodecs_decoder_init)",
				   __FUNCTION__));
		return FALSE;
	}

	/* Set frame by frame as it is natural for GStreamer data flow */
	shcodecs_decoder_set_frame_by_frame(dec->decoder,1);

	/* Autodetect Gstshvideosink */
	if(dec->use_physical == HW_ADDR_AUTO)
	{
		if(GST_IS_SH_VIDEO_SINK(gst_pad_get_parent_element(gst_pad_get_peer(dec->srcpad))))
		{
			dec->use_physical = HW_ADDR_YES;
			GST_DEBUG_OBJECT(dec,"use_physical auto detected to YES");  
		}
		else
		{
			dec->use_physical = HW_ADDR_NO;
			GST_DEBUG_OBJECT(dec,"use_physical auto detected to NO");  
		}
	}

	/* Use physical addresses for playback */
	if(dec->use_physical == HW_ADDR_YES)
	{
		shcodecs_decoder_set_use_physical(dec->decoder,1);
	}

	shcodecs_decoder_set_decoded_callback(dec->decoder,
						gst_shcodecs_decoded_callback,
						(void*)dec);

	/* Set SRC caps */
	src_caps = gst_caps_new_simple ("video/x-raw-yuv", 
					"format", GST_TYPE_FOURCC, GST_MAKE_FOURCC('N','V','1','2'),
					"framerate", GST_TYPE_FRACTION, dec->fps_numerator, dec->fps_denominator, 
					"width", G_TYPE_INT, dec->width, 
					"height", G_TYPE_INT, dec->height, 
					"framerate", GST_TYPE_FRACTION, dec->fps_numerator, dec->fps_denominator, 
					NULL);

	if(!gst_pad_set_caps(dec->srcpad,src_caps))
	{
		GST_ELEMENT_ERROR((GstElement*)dec,CORE,NEGOTIATION,
				  ("Source pad not linked."), (NULL));
		ret = FALSE;
	}
	if(!gst_pad_set_caps(gst_pad_get_peer(dec->srcpad),src_caps))
	{
		GST_ELEMENT_ERROR((GstElement*)dec,CORE,NEGOTIATION,
				  ("Peer pad not linked."), (NULL));
		ret = FALSE;
	}
	gst_caps_unref(src_caps);

	dec->caps_set = TRUE;

	GST_LOG_OBJECT(dec,"%s ok",__FUNCTION__);
	return ret;
}

static GstFlowReturn
gst_sh_video_dec_chain (GstPad * pad, GstBuffer * inbuffer)
{
	GstSHVideoDec *dec = (GstSHVideoDec *) (GST_OBJECT_PARENT (pad));
	GstFlowReturn ret = GST_FLOW_OK;

	if(!dec->caps_set)
	{
		GST_ELEMENT_ERROR((GstElement*)dec,CORE,NEGOTIATION,
				  ("Caps not set."), (NULL));
		return GST_FLOW_UNEXPECTED;
	}

	GST_LOG_OBJECT(dec,"%s called",__FUNCTION__);

	/* Checking if the new frame fits in the buffer. If it does not,
	 * we'll have to wait until the decoder has consumed the buffer. */  
	if(dec->buffer &&
	   GST_BUFFER_SIZE(dec->buffer) + GST_BUFFER_SIZE(inbuffer) > dec->buffer_size)
	{
		GST_DEBUG_OBJECT(dec,"Buffer full, waiting");    
		pthread_mutex_lock( &dec->cond_mutex );
		pthread_cond_wait( &dec->thread_condition, &dec->cond_mutex );
		pthread_mutex_unlock( &dec->cond_mutex );
		GST_DEBUG_OBJECT(dec,"Got signal");
	}

	/* Buffering */
	pthread_mutex_lock( &dec->mutex );
	if(!dec->buffer)
	{
		GST_DEBUG_OBJECT(dec,
				 "First frame in buffer. Size %d timestamp: %llu duration: %llu",
				 GST_BUFFER_SIZE(inbuffer),
				 GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP (inbuffer)),
				 GST_TIME_AS_MSECONDS(GST_BUFFER_DURATION (inbuffer)));

		dec->buffer = inbuffer;
	}  
	else
	{
		GST_LOG_OBJECT(dec,
			       "Joining buffers. Size %d timestamp: %llu duration: %llu",
			       GST_BUFFER_SIZE(inbuffer),
			       GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP (inbuffer)),
			       GST_TIME_AS_MSECONDS(GST_BUFFER_DURATION (inbuffer)));

		dec->buffer = gst_buffer_join(dec->buffer,inbuffer);
		GST_LOG_OBJECT(dec,"Buffer added. Now storing %d bytes",
			       GST_BUFFER_SIZE(dec->buffer));        
	}
	pthread_mutex_unlock( &dec->mutex );

	if(!dec->dec_thread)
	{
		GST_DEBUG_OBJECT(dec,"Starting the decoder thread");    
		dec->running = TRUE;
		pthread_create( &dec->dec_thread, NULL, gst_sh_video_dec_decode, dec);
	}

	/* Free waiting decoder */
	pthread_mutex_lock( &dec->cond_mutex );
	pthread_cond_signal( &dec->thread_condition);
	pthread_mutex_unlock( &dec->cond_mutex );

	return ret;
}

void *
gst_sh_video_dec_decode (void *data)
{
	gint used_bytes;
	GstBuffer* buffer;

	GstSHVideoDec *dec = (GstSHVideoDec *)data;

	GST_LOG_OBJECT(dec,"%s called\n",__FUNCTION__);

	/* While dec->running */
	do
	{
		/* Buffer empty, we have to wait */
		if(!dec->buffer)
		{
			GST_DEBUG_OBJECT(dec,"Waiting for data.");        
			pthread_mutex_lock( &dec->cond_mutex );
			pthread_cond_wait( &dec->thread_condition, &dec->cond_mutex );
			pthread_mutex_unlock( &dec->cond_mutex );
			GST_DEBUG_OBJECT(dec,"Got signal");        
		}

		pthread_mutex_lock(&dec->mutex);
		buffer = dec->buffer;
		dec->buffer = NULL; 
		pthread_mutex_unlock(&dec->mutex); 

		// If the other thread was waiting for buffer to be consumed
		pthread_mutex_lock( &dec->cond_mutex );
		pthread_cond_signal( &dec->thread_condition);
		pthread_mutex_unlock( &dec->cond_mutex );

		GST_DEBUG_OBJECT(dec,"Input buffer size: %d",
				 GST_BUFFER_SIZE (buffer));

		used_bytes = shcodecs_decode(dec->decoder,
				GST_BUFFER_DATA (buffer),
				GST_BUFFER_SIZE (buffer));

		GST_DEBUG_OBJECT(dec,"Used: %d",used_bytes);

		// Preserve the data that was not used
		if(GST_BUFFER_SIZE(buffer) != used_bytes)
		{    
			pthread_mutex_lock(&dec->mutex);
			if(dec->buffer)
			{
				dec->buffer = gst_buffer_join(
					 gst_buffer_create_sub(buffer,used_bytes,
							       GST_BUFFER_SIZE(buffer)-used_bytes),
					 dec->buffer);
			}
			else
			{
				dec->buffer = gst_buffer_create_sub(buffer,
								    used_bytes,
								    GST_BUFFER_SIZE(buffer)-used_bytes);
			}
			GST_DEBUG_OBJECT(dec,"Preserving %d bytes of data",
					 GST_BUFFER_SIZE(dec->buffer));
			pthread_mutex_unlock(&dec->mutex); 
		}

		if(!dec->running)
		{
			GST_DEBUG_OBJECT(dec,"We are done, calling finalize.");
			shcodecs_decoder_finalize(dec->decoder);
			GST_DEBUG_OBJECT(dec,
					 "Stream finalized. Total decoded %d frames.",
					 shcodecs_decoder_get_frame_count(dec->decoder));
		}    
		gst_buffer_unref(buffer);
		buffer = NULL;
	}while(dec->running);

	return NULL;
}

static gint
gst_shcodecs_decoded_callback (SHCodecs_Decoder * decoder,
			       guchar * y_buf, gint y_size,
			       guchar * c_buf, gint c_size,
			       void * user_data)
{
	GstSHVideoDec *dec = (GstSHVideoDec *) user_data;
	GstBuffer *buf;  
	GstFlowReturn ret;
	gint offset = shcodecs_decoder_get_frame_count(dec->decoder);

	GST_LOG_OBJECT(dec,"%s called",__FUNCTION__);  

	if(dec->use_physical == HW_ADDR_YES)
	{
		GST_LOG_OBJECT(dec,"Using own buffer");  
		buf = (GstBuffer *) gst_mini_object_new (GST_TYPE_SH_VIDEO_BUFFER);
		GST_SH_VIDEO_BUFFER_Y_DATA(buf) = y_buf;    
		GST_SH_VIDEO_BUFFER_Y_SIZE(buf) = y_size;    
		GST_SH_VIDEO_BUFFER_C_DATA(buf) = c_buf;    
		GST_SH_VIDEO_BUFFER_C_SIZE(buf) = c_size;    
		GST_BUFFER_OFFSET(buf) = offset; 
	}
	else
	{
		GST_LOG_OBJECT(dec,"Using GST buffer");  
		ret = gst_pad_alloc_buffer(dec->srcpad,offset,y_size + c_size,
					   gst_pad_get_caps(dec->srcpad),&buf);
		if (ret != GST_FLOW_OK || GST_BUFFER_SIZE(buf) != y_size + c_size) 
		{
			GST_LOG_OBJECT(dec,"Src pad didn't allocate buffer");  
			buf = gst_buffer_new_and_alloc(y_size+c_size);
			GST_BUFFER_OFFSET(buf) = offset; 
		}
		memcpy(GST_BUFFER_DATA(buf),y_buf,y_size);
		memcpy(GST_BUFFER_DATA(buf)+y_size,c_buf,c_size);
	}

	GST_BUFFER_CAPS(buf) = gst_caps_copy(GST_PAD_CAPS(dec->srcpad));
	GST_BUFFER_DURATION(buf) = GST_SECOND * dec->fps_denominator / dec->fps_numerator;
	GST_BUFFER_TIMESTAMP(buf) = offset * GST_BUFFER_DURATION(buf);
	GST_BUFFER_OFFSET_END(buf) = offset;

	GST_LOG_OBJECT (dec, "Pushing frame number: %d time: %" GST_TIME_FORMAT, 
			offset, 
			GST_TIME_ARGS (GST_BUFFER_TIMESTAMP (buf)));
	ret = gst_pad_push (dec->srcpad, buf);

	if (ret != GST_FLOW_OK) 
	{
		GST_DEBUG_OBJECT (dec, "pad_push failed: %s", gst_flow_get_name (ret));
	}

	return 0; //0 means continue decoding
}

