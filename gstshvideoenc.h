/**
 * gst-sh-mobile-enc
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
 * @author Johannes Lahti <johannes.lahti@nomovok.com>
 * @author Pablo Virolainen <pablo.virolainen@nomovok.com>
 * @author Aki Honkasuo <aki.honkasuo@nomovok.com>
 *
 */

#ifndef  GSTSHVIDEOENC_H
#define  GSTSHVIDEOENC_H

#include <gst/gst.h>
#include <shcodecs/shcodecs_encoder.h>
#include <pthread.h>

#include "cntlfile/ControlFileUtil.h"

G_BEGIN_DECLS
#define GST_TYPE_SHVIDEOENC \
  (gst_shvideo_enc_get_type())
#define GST_SHVIDEOENC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SHVIDEOENC,GstshvideoEnc))
#define GST_SHVIDEOENC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SHVIDEOENC,GstshvideoEnc))
#define GST_IS_SHVIDEOENC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SHVIDEOENC))
#define GST_IS_SHVIDEOENC_CLASS(obj) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SHVIDEOENC))
typedef struct _GstshvideoEnc GstshvideoEnc;
typedef struct _GstshvideoEncClass GstshvideoEncClass;

/**
 * Define Gstreamer SH Video Encoder structure
 */

struct _GstshvideoEnc
{
  GstElement element;
  GstPad *sinkpad, *srcpad;
  GstBuffer *buffer_yuv;
  GstBuffer *buffer_cbcr;

  gint offset;
  SHCodecs_Format format;  
  SHCodecs_Encoder* encoder;
  gint width;
  gint height;
  gint fps_numerator;
  gint fps_denominator;

  APPLI_INFO ainfo;
  
  GstCaps* out_caps;
  gboolean caps_set;
  glong frame_number;
  GstClockTime timestamp_offset;

  gboolean stream_paused;

  pthread_t enc_thread;
  pthread_mutex_t mutex;
  pthread_mutex_t cond_mutex;
  pthread_cond_t  thread_condition;
};

/**
 * Define Gstreamer SH Video Encoder Class structure
 */

struct _GstshvideoEncClass
{
  GstElementClass parent;
};

/** Get gst-sh-mobile-enc object type
    @return object type
*/

GType gst_shvideo_enc_get_type (void);

/** Initializes the SH Hardware encoder
    @param shvideoenc encoder object
*/

void gst_shvideo_enc_init_encoder(GstshvideoEnc * shvideoenc);

/** Reads the capabilities of the peer element behind source pad
    @param shvideoenc encoder object
*/

void gst_shvideoenc_read_src_caps(GstshvideoEnc * shvideoenc);

/** Sets the capabilities of the source pad
    @param shvideoenc encoder object
    @return TRUE if the capabilities could be set, otherwise FALSE
*/

gboolean gst_shvideoenc_set_src_caps(GstshvideoEnc * shvideoenc);

/** Launches the encoder in an own thread
    @param data encoder object
*/

void* launch_encoder_thread(void *data);

G_END_DECLS
#endif
