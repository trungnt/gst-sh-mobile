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
#define GST_TYPE_SHVIDEOSINK \
  (gst_shvideosink_get_type())
#define GST_SHVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SHVIDEOSINK,Gstshvideosink))
#define GST_SHVIDEOSINK_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SHVIDEOSINK,Gstshvideosink))
#define GST_IS_SHVIDEOSINK(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SHVIDEOSINK))
#define GST_IS_SHVIDEOSINK_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SHVIDEOSINK))
typedef struct _Gstshvideosink Gstshvideosink;
typedef struct _GstshvideosinkClass GstshvideosinkClass;

/**
 * GST_SHVIDEOSINK_CAST:
 * @obj: a #Gstshvideosink
 *
 * Cast @obj to a #Gstshvideosink without runtime type check.
 *
 */
#define GST_SHVIDEOSINK_CAST(obj)  ((Gstshvideosink *) (obj))

/**
 * Define Gstreamer SH Video Sink structure
 */

struct _Gstshvideosink
{
  GstVideoSink video_sink;

  /* Input stream */
  gint fps_numerator;
  gint fps_denominator;

  gboolean caps_set;
  
  /* Output */

  gint dst_width;
  gint dst_height;
  gint dst_x;
  gint dst_y;

  gint zoom_factor;
  
  framebuffer fb;
  uio_module veu;
};

/**
 * Define Gstreamer SH Video Sink Class structure
 */

struct _GstshvideosinkClass
{
  GstVideoSinkClass parent_class;
};


/** Get gst-sh-mobile-sink object type
    @return object type
*/

GType gst_shvideosink_get_type (void);

G_END_DECLS
#endif
