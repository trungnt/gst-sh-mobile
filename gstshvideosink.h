/**
 * gst-sh-mobile-sink
 *
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
 * @author Pablo Virolainen <pablo.virolainen@nomovok.com>
 * @author Johannes Lahti <johannes.lahti@nomovok.com>
 * @author Aki Honkasuo <aki.honkasuo@nomovok.com>
 *
 */


#ifndef  GSTSHVIDEOSINK_H
#define  GSTSHVIDEOSINK_H

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/gstelement.h>
#include "gstshioutils.h"

G_BEGIN_DECLS
#define GST_TYPE_SH_VIDEO_SINK \
	(gst_sh_video_sink_get_type())
#define GST_SH_VIDEO_SINK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SH_VIDEO_SINK,GstSHVideoSink))
#define GST_SH_VIDEO_SINK_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SH_VIDEO_SINK,GstSHVideoSink))
#define GST_IS_SH_VIDEO_SINK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SH_VIDEO_SINK))
#define GST_IS_SH_VIDEO_SINK_CLASS(obj) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SH_VIDEO_SINK))
typedef struct _GstSHVideoSink GstSHVideoSink;
typedef struct _GstSHVideoSinkClass GstSHVideoSinkClass;

#define GST_SH_VIDEO_SINK_CAST(obj)  ((GstSHVideoSink *) (obj))

/**
 * \struct _GstSHVideoSink
 * \var video_sink Parent element
 * \var fps_numerator Numerator of the framerate fraction
 * \var fps_denominator Denominator of the framerate fraction
 * \var caps_set indicates whether the caps of the sink elements have been set
 * \var dst_width Width of the output
 * \var dst_width Height of the output
 * \var dst_x X-ccordinate of the output
 * \var dst_y Y-coordinate of the output
 * \var zoom_factor Zoom -setting. (See properties)
 * \var fb Framebuffer
 * \var veu VEU (Video Engine Unit)
 */
struct _GstSHVideoSink
{
	GstVideoSink video_sink;

	gint fps_numerator;
	gint fps_denominator;

	gboolean caps_set;
	
	gint dst_width;
	gint dst_height;
	gint dst_x;
	gint dst_y;
	gint zoom_factor;
	
	framebuffer fb;
	uio_module veu;
};

/**
 * \struct _GstSHVideoSinkClass
 * \var parent_class Parent
 */
struct _GstSHVideoSinkClass
{
	GstVideoSinkClass parent_class;
};


/** 
* Get gst-sh-mobile-sink object type
* @return object type
*/
GType gst_sh_video_sink_get_type (void);

G_END_DECLS
#endif
