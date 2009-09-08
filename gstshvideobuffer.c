/**
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
 * \author Pablo Virolainen <pablo.virolainen@nomovok.com>
 * \author Johannes Lahti <johannes.lahti@nomovok.com>
 * \author Aki Honkasuo <aki.honkasuo@nomovok.com>
 *
 */

#include "gstshvideobuffer.h"

static GstBufferClass *parent_class;

/** 
 * Initialize the buffer
 * \param shbuffer GstSHVideoBuffer object
 * \param g_class GClass pointer
 */
static void
gst_sh_video_buffer_init (GstSHVideoBuffer * shbuffer, gpointer g_class)
{
	GST_SH_VIDEO_BUFFER_Y_DATA(shbuffer) = NULL;
	GST_SH_VIDEO_BUFFER_Y_SIZE(shbuffer) = 0;
	GST_SH_VIDEO_BUFFER_C_DATA(shbuffer) = NULL;
	GST_SH_VIDEO_BUFFER_C_SIZE(shbuffer) = 0;
}

/** 
 * Finalize the buffer
 * \param shbuffer GstSHVideoBuffer object
 */
static void
gst_sh_video_buffer_finalize (GstSHVideoBuffer * shbuffer)
{
	/* Set malloc_data to NULL to prevent parent class finalize
	* from trying to free allocated data. This buffer is used only in HW
	* address space, where we don't allocate data but just map it. 
	*/
	GST_BUFFER_MALLOCDATA(shbuffer) = NULL;
	GST_BUFFER_DATA(shbuffer) = NULL;
	GST_SH_VIDEO_BUFFER_C_DATA(shbuffer) = NULL;
	GST_SH_VIDEO_BUFFER_C_SIZE(shbuffer) = 0;

	GST_MINI_OBJECT_CLASS (parent_class)->finalize (GST_MINI_OBJECT (shbuffer));
}

/** 
 * Initialize the buffer class
 * \param g_class GClass pointer
 * \param class_data Optional data pointer
 */
static void
gst_sh_video_buffer_class_init (gpointer g_class, gpointer class_data)
{
	GstMiniObjectClass *mini_object_class = GST_MINI_OBJECT_CLASS (g_class);

	parent_class = g_type_class_peek_parent (g_class);

	mini_object_class->finalize = (GstMiniObjectFinalizeFunction)
			gst_sh_video_buffer_finalize;
}

GType
gst_sh_video_buffer_get_type (void)
{
	static GType gst_sh_video_buffer_type;

	if (G_UNLIKELY (gst_sh_video_buffer_type == 0)) {
		static const GTypeInfo gst_sh_video_buffer_info = {
			sizeof (GstBufferClass),
			NULL,
			NULL,
			gst_sh_video_buffer_class_init,
			NULL,
			NULL,
			sizeof (GstSHVideoBuffer),
			0,
			(GInstanceInitFunc) gst_sh_video_buffer_init,
			NULL
		};
		gst_sh_video_buffer_type = g_type_register_static (GST_TYPE_BUFFER,
				"GstSHVideoBuffer", &gst_sh_video_buffer_info, 0);
	}
	return gst_sh_video_buffer_type;
}
