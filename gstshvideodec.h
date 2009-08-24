/**
 * gst-sh-mobile-dec-sink
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


#ifndef  GSTSHVIDEODEC_H
#define  GSTSHVIDEODEC_H

#include <gst/gst.h>
#include <gst/video/gstvideosink.h>
#include <gst/gstelement.h>

G_BEGIN_DECLS
#define GST_TYPE_SHVIDEODEC \
  (gst_shvideodec_get_type())
#define GST_SHVIDEODEC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SHVIDEODEC,Gstshvideodec))
#define GST_SHVIDEODEC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SHVIDEODEC,Gstshvideodec))
#define GST_IS_SHVIDEODEC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SHVIDEODEC))
#define GST_IS_SHVIDEODEC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SHVIDEODEC))
typedef struct _Gstshvideodec Gstshvideodec;
typedef struct _GstshvideodecClass GstshvideodecClass;

#include <shcodecs/shcodecs_decoder.h>

/**
 * Define Gstreamer SH Video Decoder structure
 */

struct _Gstshvideodec
{
  GstElement element;

  GstPad *sinkpad;
  GstPad *srcpad;

  /* Input stream */
  SHCodecs_Format format;
  gint width;
  gint height;
  gint fps_numerator;
  gint fps_denominator;
  SHCodecs_Decoder * decoder;

  gboolean caps_set;
  gboolean running;
  
  gint use_physical;  

  GstBuffer* buffer;
  guint32 buffer_size;
  GstClockTime buffer_timestamp;
  gint buffer_frames;

  GstClockTime decode_timestamp;
  gint decode_frames;  
  gint decoded;

  pthread_t dec_thread;
  pthread_mutex_t mutex;
  pthread_mutex_t cond_mutex;
  pthread_cond_t  thread_condition;
};

/**
 * Define Gstreamer SH Video Decoder Class structure
 */

struct _GstshvideodecClass
{
  GstElementClass parent;
};

/** Get gst-sh-mobile-dec-sink object type
    @return object type
*/

GType gst_shvideodec_get_type (void);

/** The video input buffer decode function
    @param data decoder object
*/

void* gst_shvideodec_decode (void *data);

G_END_DECLS
#endif
