/**
 * \page sink gst-sh-mobile-sink
 * gst-sh-mobile-sink - Video sink on SuperH environment.
 *
 * \section sink-description Description
 * Basic video sink for the Renesas SuperH chipset.
 * 
 * \section sink-examples Example launch lines
 *
 * \subsection sink-example-1 Displaying a test stream
 *
 * \code
 * gst-launch videotestsrc !  gst-sh-mobile-sink
 * \endcode
 * This is the simplest pipeline which can be used to test the 
 * gst-sh-mobile-sink element. Videotestsrc provides video streams in various 
 * formats for testing.
 *
 *
 * \subsection sink-example-2 Displaying an Ogg video
 *
 * \code
 * gst-launch filesrc location=/mnt/usb/theora-test.ogg ! oggdemux ! theoradec
 * ! ffmpegcolorspace ! gst-sh-mobile-sink width=400 height=240 x=100 y=100
 * \endcode
 * OGG video stream read from a file using filesrc element and then demuxed with
 * oggdemux. The  Theora video elementary stream is then decoded using theoradec
 * element. Output is scaled to 400x240 resolution and top-left corner is placed
 * at 100:100 coordinates using the properties of gst-sh-mobile-sink.
 *
 *
 * \subsection sink-example-3 Displaying an AVI video + audio playback
 *
 * \code
 * gst-launch filesrc location=test.avi ! avidemux name=demux demux.audio_00
 * ! queue ! decodebin ! audioconvert ! audioresample ! autoaudiosink
 * demux.video_00 ! queue ! gst-sh-mobile-dec ! gst-sh-mobile-sink
 * \endcode
 *
 * \image html decoder_example.jpeg
 *
 * Filesrc element is used to read the file, which this time is an avi
 * wrapped video containing both audio and video streams. avidemux element is
 * used to strip the avi container. avidemux has two src-pads, which are
 * named “demux” using a property. Both of the avidemux src pads are first
 * connected to queue elements, which take care of the buffering of the data in
 * the pipeline. 
 *
 * The audio stream is then connected to the the decodebin element, which detects
 * the stream format and does the decoding. audioconvert and audioresample
 * elements are used to transform the data into a suitable format for
 * playback. The last element in the audio pipeline is autoaudiosink, which
 * automatically detects and connects the correct audio sink for playback. This
 * audio pipeline composition is very common in GStreamer programming.
 * 
 * The video pipeline is constructed from SuperH specific elements:
 * gst-sh-mobile-dec and gst-sh-mobile-sink. The gst-sh-mobile-sink is
 * a videosink element for SuperH.
 *
 * \section sink-properties Properties
 * \copydoc gstshvideosinkproperties
 *
 * \section sink-pads Pads
 * \copydoc gst_sh_video_sink_sink_template_factory
 * 
 * \section sink-license Licensing
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
#include <string.h>

#include "gstshvideosink.h"
#include "gstshvideobuffer.h"

GST_DEBUG_CATEGORY_STATIC (gst_sh_video_sink_debug);
#define GST_CAT_DEFAULT gst_sh_video_sink_debug

//Minimum size for the output width/height
#define MIN_W_AND_H 16

/**
 * \var gst_sh_video_sink_sink_template_factory
 * Name: sink \n
 * Direction: sink \n
 * Available: always \n
 * Caps:
 * - video/x-raw-yuv, format=(fourcc)NV12, width=(int)[16,2560], 
 *   height=(int)[16,1920], framerate=(fraction)[1,30]
 */
static GstStaticPadTemplate gst_sh_video_sink_sink_template_factory =
GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (
				 "video/x-raw-yuv, "
				 "format = (fourcc) NV12,"
				 "framerate = (fraction) [1, 30],"
				 "width = (int) [16, 2560],"
				 "height = (int) [16, 1920]" 
				 )
		);

/**
 * \enum gstshvideosinkproperties
 * gst-sh-mobile-sink has following properties:
 * - "width" (int). Width of the video output. Default: original width.
 * - "height" (int). Height of the video output. Default: original height.
 * - "x" (int). X-coordinate of the video output. Default: 0.
 * - "y" (int). Y-coordinate of the video output. Default: 0.
 * - "zoom" (string). Zoom factor of the video output. Possible values:
 *   "orig"/"full"/"double"/"half". Default: "orig"
 */
enum gstshvideosinkproperties
{
	PROP_0,
	PROP_WIDTH,
	PROP_HEIGHT,
	PROP_X,
	PROP_Y,
	PROP_ZOOM
};

enum
{
	ZOOM_ORIG,
	ZOOM_FULL,
	ZOOM_DOUBLE,
	ZOOM_HALF
};

#define ZOOM_FACTOR_ORIG "orig"
#define ZOOM_FACTOR_FULL "full"
#define ZOOM_FACTOR_DOUBLE "double"
#define ZOOM_FACTOR_HALF "half"

static GstVideoSinkClass *parent_class = NULL;

/** 
 * Initialize SH hardware video sink
 * \param klass Gstreamer element class
 */
static void gst_sh_video_sink_base_init (gpointer klass);

/** 
 * Dispose sink
 * \param object Gstreamer element class
 */
static void gst_sh_video_sink_dispose (GObject * object);

/** 
 * Initialize the class
 * \param klass Gstreamer SH video sink class
 */
static void gst_sh_video_sink_class_init (GstSHVideoSinkClass * klass);

/** 
 * Initialize the sink
 * \param sink Gstreamer SH sink element
 * \param gklass Gstreamer SH video sink class
 */
static void gst_sh_video_sink_init (GstSHVideoSink * sink, GstSHVideoSinkClass * gklass);

/** 
 * The function will set the properties of the sink
 * \param object The object where to get Gstreamer SH video Sink object
 * \param prop_id The property id
 * \param value The value of the prioperty
 * \param pspec not used in fuction
 */
static void gst_sh_video_sink_set_property (GObject *object, 
					  guint prop_id, const GValue *value, 
					  GParamSpec * pspec);

/** 
 * The function will return the wanted property of the sink
 * \param object The object where to get Gstreamer SH video Sink object
 * \param prop_id The property id
 * \param value The value of the property
 * \param pspec not used in fuction
 */
static void gst_sh_video_sink_get_property (GObject * object, guint prop_id,
					  GValue * value, GParamSpec * pspec);

/**
 * From GstBaseSink. All non stream depending resources are allocated
 * here.
 * \param bsink GstBaseSink element
 * \return true if no errors
 */
static gboolean gst_sh_video_sink_start (GstBaseSink *bsink);

/**
 * From GstBaseSink. All non stream depending resources are released
 * here.
 * \param bsink GstBaseSink element
 * \return true if no errors
 */
static gboolean gst_sh_video_sink_stop (GstBaseSink *sink);

/**
 * Returns the caps fron sink template.
 * \param bsink GstBaseSink element
 * \return the caps
 */
static GstCaps* gst_sh_video_sink_getcaps (GstBaseSink * bsink);

/** 
 * Initialize the sink pad 
 * \param bsink Gstreamer BaseSink object
 * \param caps The capabilities of the video to play
 * \return returns true if the video capatilies are supported and the video can be played
 */
static gboolean gst_sh_video_sink_setcaps (GstBaseSink * bsink, GstCaps * caps);

/**
 * From GstBaseSink. Here we can control when frames are played.
 * \param bsink GstBaseSink element
 * \param buf The next buffer to be displayed
 * \param start Starting time of the frame will be returned here.
 * \param end Ending time of the frame will be returned here.
 */
static void gst_sh_video_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
				       GstClockTime * start, 
				       GstClockTime * end);

/**
 * From GstBaseSink. Called when it is time to display a new frame.
 * \param bsink GstBaseSink element
 * \param buf The frame to be displayed
 * \return Returns GST_FLOW_OK if frame was shown. Otherwise GST_FLOW_ERROR
 */
static GstFlowReturn gst_sh_video_sink_show_frame (GstBaseSink * bsink, 
						   GstBuffer * buf);

/**
 * From GstBaseSink. Allows upstream buffer allocation. Here this is used to
 * allocate memory from VEU memory, which saves one memcpy call.
 * \param bsink GstBaseSink element
 * \param offset Offset of the buffer
 * \param size Size of the buffer
 * \param caps Caps for the buffer. This could be used for upstream caps
 *             negotiation.
 * \param buf The allocated buffer is returned using this pointer
 * \return Returns GST_FLOW_OK if frame was shown. Otherwise GST_FLOW_ERROR
 */

static GstFlowReturn gst_sh_video_sink_buffer_alloc (GstBaseSink *bsink, 
						     guint64 offset, guint size,
						     GstCaps *caps, GstBuffer **buf);





GType
gst_sh_video_sink_get_type (void)
{
	static GType object_type = 0;

	if (object_type == 0) 
	{
		static const GTypeInfo object_info = 
		{
			sizeof (GstSHVideoSinkClass),
			gst_sh_video_sink_base_init,
			NULL,
			(GClassInitFunc) gst_sh_video_sink_class_init,
			NULL,
			NULL,
			sizeof (GstSHVideoSink),
			0,
			(GInstanceInitFunc) gst_sh_video_sink_init
		};

		object_type = g_type_register_static (GST_TYPE_VIDEO_SINK, 
						      "gst-sh-mobile-sink", 
						      &object_info,
						      (GTypeFlags) 0);
	}
	return object_type;
}

static void
gst_sh_video_sink_base_init (gpointer g_class)
{
	static const GstElementDetails plugin_details =
		GST_ELEMENT_DETAILS ("SuperH video sink",
			 	     "Sink/Video",
			 	     "Playback video in SuperH environment.",
			 	     "Johannes Lahti <johannes.lahti@nomovok.com>");

	GstElementClass *element_class = GST_ELEMENT_CLASS (g_class);

	gst_element_class_add_pad_template (element_class,
				gst_static_pad_template_get (&gst_sh_video_sink_sink_template_factory));
	gst_element_class_set_details (element_class, &plugin_details);
}

static void
gst_sh_video_sink_dispose (GObject * object)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (object);

	GST_LOG_OBJECT(sink,"%s called\n",__FUNCTION__);  

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_sh_video_sink_class_init (GstSHVideoSinkClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;
	GstBaseSinkClass *gstbasesink_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;
	gstbasesink_class = (GstBaseSinkClass *) klass;

	GST_DEBUG_CATEGORY_INIT (gst_sh_video_sink_debug, 
				 "gst-sh-mobile-sink",
				 0, "Player for raw video streams");

	parent_class = g_type_class_peek_parent (klass);

	gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_sh_video_sink_dispose);
	gobject_class->set_property = gst_sh_video_sink_set_property;
	gobject_class->get_property = gst_sh_video_sink_get_property;

	g_object_class_install_property (gobject_class, PROP_WIDTH,
			g_param_spec_uint ("width", "Playback width", 
			"Width of the video frame on the display",
			0, G_MAXUINT, 0, 
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_HEIGHT,
			g_param_spec_uint ("height", "Playback height", 
			"Height of the video frame on the display", 
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_X,
			g_param_spec_uint ("x", "x coordinate on screen", 
			"x -coordinate of the video frame on the display", 
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_Y,
			g_param_spec_uint ("y", "y coordinate on screen", 
			"y -coordinate of the video frame on the display", 
			0, G_MAXUINT, 0,
			G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	g_object_class_install_property (gobject_class, PROP_ZOOM,
			g_param_spec_string ("zoom", "Zoom level", 
				 "Output zoom level(original(default)/full/double/half)",
				 NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

	gstbasesink_class->set_caps = GST_DEBUG_FUNCPTR (gst_sh_video_sink_setcaps);
	gstbasesink_class->get_caps = GST_DEBUG_FUNCPTR (gst_sh_video_sink_getcaps);
	gstbasesink_class->get_times =
			GST_DEBUG_FUNCPTR (gst_sh_video_sink_get_times);
	gstbasesink_class->preroll = GST_DEBUG_FUNCPTR (gst_sh_video_sink_show_frame);
	gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_sh_video_sink_show_frame);
	gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_sh_video_sink_start);
	gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_sh_video_sink_stop);

	gstbasesink_class->buffer_alloc = GST_DEBUG_FUNCPTR (gst_sh_video_sink_buffer_alloc);
}

static void
gst_sh_video_sink_init (GstSHVideoSink * sink, GstSHVideoSinkClass * gklass)
{
	GST_LOG_OBJECT(sink,"%s called",__FUNCTION__);

	sink->fps_numerator = 0;
	sink->fps_denominator = 1;

	sink->caps_set = FALSE;

	sink->zoom_factor = ZOOM_ORIG;
	
	sink->dst_width = 0;
	sink->dst_height = 0;
	sink->dst_x = 0;
	sink->dst_y = 0;
}


static void
gst_sh_video_sink_set_property (GObject * object, guint prop_id,
			      const GValue * value, GParamSpec * pspec)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (object);
	const gchar* string;
	
	GST_LOG_OBJECT(sink,"%s called",__FUNCTION__);

	switch (prop_id) 
	{
		case PROP_WIDTH:
		{
			sink->dst_width = g_value_get_uint (value);
			break;
		}
		case PROP_HEIGHT:
		{
			sink->dst_height = g_value_get_uint (value);
			break;
		}
		case PROP_X:
		{
			sink->dst_x = g_value_get_uint (value);
			break;
		}
		case PROP_Y:
		{
			sink->dst_y = g_value_get_uint (value);
			break;
		}
		case PROP_ZOOM:
		{
			string = g_value_get_string (value);

			if(!strcmp(string,ZOOM_FACTOR_ORIG))
			{
				sink->zoom_factor = ZOOM_ORIG;
			}
			else if(!strcmp(string,ZOOM_FACTOR_FULL))
			{
				sink->zoom_factor = ZOOM_FULL;
			}
			else if(!strcmp(string,ZOOM_FACTOR_DOUBLE))
			{
				sink->zoom_factor = ZOOM_DOUBLE;
			}
			else if(!strcmp(string,ZOOM_FACTOR_HALF))
			{
				sink->zoom_factor = ZOOM_HALF;
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
gst_sh_video_sink_get_property (GObject * object, guint prop_id,
			      GValue * value, GParamSpec * pspec)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (object);

	GST_DEBUG_OBJECT(sink,"%s called",__FUNCTION__);

	switch (prop_id) 
	{
		case PROP_WIDTH:
		{
			g_value_set_uint (value,sink->dst_width);
			break;
		}
		case PROP_HEIGHT:
		{
			g_value_set_uint (value,sink->dst_height);
			break;
		}
		case PROP_X:
		{
			g_value_set_uint (value,sink->dst_x);
			break;
		}
		case PROP_Y:
		{
			g_value_set_uint (value,sink->dst_y);
			break;
		}
		case PROP_ZOOM:
		{
			switch(sink->zoom_factor)
			{
				case ZOOM_ORIG:
				{
					g_value_set_string(value,ZOOM_FACTOR_ORIG);
					break;
				}
				case ZOOM_FULL:
				{
					g_value_set_string(value,ZOOM_FACTOR_FULL);
					break;
				}
				case ZOOM_DOUBLE:
				{
					g_value_set_string(value,ZOOM_FACTOR_DOUBLE);
					break;
				}
				case ZOOM_HALF:
				{
					g_value_set_string(value,ZOOM_FACTOR_HALF);
					break;
				}
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

static gboolean 
gst_sh_video_sink_start (GstBaseSink *bsink)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);
	
	GST_DEBUG_OBJECT(sink,"START, opening devices.");

	if(!init_framebuffer(&sink->fb))
	{
		GST_ELEMENT_ERROR((GstElement*)sink,
			CORE,FAILED,("Failed to init framebuffer."), 
			("%s failed (Failed to init framebuffer)",
			__FUNCTION__));            
	}
	GST_DEBUG_OBJECT(sink,"Framebuffer: %dx%d %dbpp.",sink->fb.vinfo.xres,
		sink->fb.vinfo.yres,sink->fb.vinfo.bits_per_pixel);

	if(!init_veu(&sink->veu))
	{
		GST_ELEMENT_ERROR((GstElement*)sink,
			CORE,FAILED,("Failed to init VEU."), 
			("%s failed (Failed to init VEU)",__FUNCTION__));
	}
	GST_DEBUG_OBJECT(sink,"VEU, name: %s path: %s",
			sink->veu.dev.name, sink->veu.dev.path);

	return TRUE;
}

static gboolean 
gst_sh_video_sink_stop (GstBaseSink *bsink)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);

	GST_DEBUG_OBJECT(sink,"STOP, closing devices.");
	clear_framebuffer(&sink->fb);

	return TRUE;
}

static GstCaps *
gst_sh_video_sink_getcaps (GstBaseSink * bsink)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);

	GST_LOG_OBJECT(sink,"%s called",__FUNCTION__);

	return gst_caps_copy (gst_pad_get_pad_template_caps (bsink->sinkpad));
}

static gboolean
gst_sh_video_sink_setcaps (GstBaseSink * bsink, GstCaps * caps)
{
	GstStructure *structure = NULL;
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);

	GST_DEBUG_OBJECT(sink,"%s called",__FUNCTION__);

	structure = gst_caps_get_structure (caps, 0);

	if(!gst_structure_get_fraction (structure, "framerate", 
					&sink->fps_numerator, 
					&sink->fps_denominator))
	{
		GST_DEBUG_OBJECT(sink,"%s failed (no framerate)",__FUNCTION__);
		return FALSE;
	}

	if (!(gst_structure_get_int (structure, "width",  
				     &sink->video_sink.width)
	      && gst_structure_get_int (structure, 
				        "height", &sink->video_sink.height))) 
	{
		GST_DEBUG_OBJECT(sink,"%s failed (no width/height)",
				 __FUNCTION__);
		return FALSE;
	}

	GST_DEBUG_OBJECT(sink,"Caps set. Framerate: %d/%d width: %d height: %d"
			 ,sink->fps_numerator,sink->fps_denominator,
			 sink->video_sink.width,
			 sink->video_sink.height);

	if(!sink->dst_width && !sink->dst_height 
	   && sink->zoom_factor != ZOOM_ORIG)
	{
		switch(sink->zoom_factor)
		{
			case ZOOM_FULL:
			{
				sink->dst_width = sink->fb.vinfo.xres;
				sink->dst_height = sink->fb.vinfo.yres;
				break;
			}
			case ZOOM_DOUBLE:
			{
				sink->dst_width = sink->video_sink.width * 2;
				sink->dst_height = sink->video_sink.height * 2;
				break;
			}
			case ZOOM_HALF:
			{
				sink->dst_width = sink->video_sink.width / 2;
				sink->dst_height = sink->video_sink.height / 2;
				break;
			}
		}
	}  

	if(!sink->dst_width)
	{
		sink->dst_width = sink->video_sink.width;
	}
	if(!sink->dst_height)
	{
		sink->dst_height = sink->video_sink.height;
	}

	if(sink->dst_width > sink->fb.vinfo.xres)
	{
		sink->dst_width = sink->fb.vinfo.xres;
	}
	if(sink->dst_height > sink->fb.vinfo.yres)
	{
		sink->dst_height = sink->fb.vinfo.yres;
	}

	if(sink->dst_width < MIN_W_AND_H)
	{
		sink->dst_width = MIN_W_AND_H;
	}
	if(sink->dst_height < MIN_W_AND_H)
	{
		sink->dst_height = MIN_W_AND_H;
	}

	if(!setup_veu(&sink->veu, sink->video_sink.width, 
		      sink->video_sink.height, sink->dst_width, 
		      sink->dst_height, sink->fb.finfo.line_length,
		      sink->dst_x,sink->dst_y, sink->fb.vinfo.xres, 
		      sink->fb.vinfo.yres, sink->fb.finfo.smem_start,
		      sink->fb.vinfo.bits_per_pixel))
	{
		GST_ELEMENT_ERROR((GstElement*)sink,
			CORE,FAILED,("Failed to setup VEU."), 
			("%s failed (Failed to setup VEU)",__FUNCTION__));
	}


	GST_INFO_OBJECT(sink,
			"VEU setup: %dx%d->%dx%d @%d:%d line:%d screen:%dx%d bpp:%d addr:%lx",
			sink->video_sink.width, sink->video_sink.height, 
			sink->dst_width, sink->dst_height, sink->dst_x,
			sink->dst_y, sink->fb.finfo.line_length,
			sink->fb.vinfo.xres, sink->fb.vinfo.yres, 
			sink->fb.vinfo.bits_per_pixel,
			sink->fb.finfo.smem_start		   
			 );

	sink->caps_set = TRUE;

	GST_LOG_OBJECT(sink,"%s ok",__FUNCTION__);

	return TRUE;
}

static void
gst_sh_video_sink_get_times (GstBaseSink * bsink, GstBuffer * buf,
			   GstClockTime * start, GstClockTime * end)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);

	GST_LOG_OBJECT(sink,"Times requested. Buffer timestamp: %dms duration:%dms",
		 GST_TIME_AS_MSECONDS(GST_BUFFER_TIMESTAMP (buf)),
		 GST_TIME_AS_MSECONDS(GST_BUFFER_DURATION (buf)));

	if (GST_BUFFER_TIMESTAMP_IS_VALID (buf)) 
	{
		*start = GST_BUFFER_TIMESTAMP (buf);
		if (GST_BUFFER_DURATION_IS_VALID (buf) && 
	GST_TIME_AS_MSECONDS(GST_BUFFER_DURATION (buf)) > 0 )
		{
			*end = *start + GST_BUFFER_DURATION (buf);
		} 
		else 
		{
			if (sink->fps_numerator > 0) 
			{
				*end = *start +
				 	gst_util_uint64_scale_int (GST_SECOND,
 						sink->fps_denominator,
						sink->fps_numerator);
			}
			else 
			{
		 		GST_ELEMENT_ERROR((GstElement*)sink,
		 			CORE,FAILED,("No framerate set."), 
			 	("%s failed (No framerate set for playback)",
				__FUNCTION__));        
			}
		}
		GST_LOG_OBJECT(sink,"Times given, start: %dms end:%dms",
			 GST_TIME_AS_MSECONDS(*start),
			 GST_TIME_AS_MSECONDS(*end));
	}
	else 
	{
		GST_ELEMENT_ERROR((GstElement*)sink,
			CORE,FAILED,("No timestamp given."), 
			("%s failed (No timestamp set for the buffer)",
			__FUNCTION__));        
	}
}

static GstFlowReturn
gst_sh_video_sink_show_frame (GstBaseSink * bsink, GstBuffer * buf)
{
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);

	GST_LOG_OBJECT(sink,"%s called",__FUNCTION__);

	g_return_val_if_fail (buf != NULL, GST_FLOW_ERROR);

	if(GST_IS_SH_VIDEO_BUFFER(buf))
	{
		GST_LOG_OBJECT(sink,"Got own buffer with HW adrresses");
			veu_blit(&sink->veu, 
			(unsigned long)GST_SH_VIDEO_BUFFER_Y_DATA(buf), 
			(unsigned long)GST_SH_VIDEO_BUFFER_C_DATA(buf));
	}
	else
	{
		GST_LOG_OBJECT(sink,"Got userland buffer -> memcpy");
		memcpy(sink->veu.mem.iomem,GST_BUFFER_DATA(buf),
			GST_BUFFER_SIZE(buf));
		veu_blit(&sink->veu,sink->veu.mem.address,
			sink->veu.mem.address + 
			(sink->video_sink.width*sink->video_sink.height));
	}

    veu_wait_irq(&sink->veu);    

	return GST_FLOW_OK;
}


static GstFlowReturn 
gst_sh_video_sink_buffer_alloc (GstBaseSink *bsink, guint64 offset, guint size,
				GstCaps *caps, GstBuffer **buf)
{
	GstStructure *structure = NULL;
	GstSHVideoSink *sink = GST_SH_VIDEO_SINK (bsink);
	gint width, height;

	GST_LOG_OBJECT(sink,"Buffer requested. Offset: %lld, size: %d",offset,size);

	if(size<=0)
	{
		GST_DEBUG_OBJECT(sink,"%s failed (no size for buffer)",__FUNCTION__);
		return GST_FLOW_UNEXPECTED;
	}  

	if(size > sink->veu.mem.size)
	{
		GST_DEBUG_OBJECT(sink,"%s not enough space in VEU mem",__FUNCTION__);
		return GST_FLOW_UNEXPECTED;
	}  

	structure = gst_caps_get_structure (caps, 0);

	if (!(gst_structure_get_int (structure, "width",  &width)
	&& gst_structure_get_int (structure, "height", &height))) 
	{
		GST_DEBUG_OBJECT(sink,"%s failed (no width/height)",__FUNCTION__);
		return GST_FLOW_UNEXPECTED;
	}

	GST_LOG_OBJECT(sink,"Frame width: %d heigth: %d",width,height);

        // Using HW buffer from VEU
	*buf = (GstBuffer *) gst_mini_object_new (GST_TYPE_SH_VIDEO_BUFFER);
	GST_BUFFER_DATA(*buf) = GST_BUFFER_MALLOCDATA(*buf) = sink->veu.mem.iomem;    
	GST_BUFFER_SIZE(*buf) = size;    
	GST_SH_VIDEO_BUFFER_Y_DATA(*buf) = (guint8*)sink->veu.mem.address; 
	GST_SH_VIDEO_BUFFER_Y_SIZE(*buf) = width*height; 
	GST_SH_VIDEO_BUFFER_C_DATA(*buf) = (guint8*)(sink->veu.mem.address+GST_SH_VIDEO_BUFFER_Y_SIZE(*buf)); 
	GST_SH_VIDEO_BUFFER_C_SIZE(*buf) = GST_SH_VIDEO_BUFFER_Y_SIZE(*buf)/2; 

	return GST_FLOW_OK;
}
