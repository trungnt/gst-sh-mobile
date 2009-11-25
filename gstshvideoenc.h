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
#define GST_TYPE_SH_VIDEO_ENC \
	(gst_sh_video_enc_get_type())
#define GST_SH_VIDEO_ENC(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SH_VIDEO_ENC,GstSHVideoEnc))
#define GST_SH_VIDEO_ENC_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SH_VIDEO_ENC,GstSHVideoEnc))
#define GST_IS_SH_VIDEO_ENC(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SH_VIDEO_ENC))
#define GST_IS_SH_VIDEO_ENC_CLASS(obj) \
	(G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SH_VIDEO_ENC))
typedef struct _GstSHVideoEnc GstSHVideoEnc;
typedef struct _GstSHVideoEncClass GstSHVideoEncClass;

/**
 * Define Gstreamer SH Video Encoder structure
 */
struct _GstSHVideoEnc
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

	gboolean stream_stopped;
	gboolean eos;

	pthread_t enc_thread;
	pthread_mutex_t mutex;
	pthread_mutex_t cond_mutex;
	pthread_cond_t  thread_condition;

	/* PROPERTIES */
	/* common */
	glong bitrate;
	glong i_vop_interval;
	glong mv_mode;
	glong fcode_forward;
	glong search_mode;
	glong search_time_fixed;
	glong ratecontrol_skip_enable;
	glong ratecontrol_use_prevquant;
	glong ratecontrol_respect_type;
	glong ratecontrol_intra_thr_changeable;
	glong control_bitrate_length;
	glong intra_macroblock_refresh_cycle;
	glong video_format;
	glong frame_num_resolution;
	glong noise_reduction;
	glong reaction_param_coeff;
	glong weighted_q_mode;
	gulong i_vop_quant_initial_value;
	gulong p_vop_quant_initial_value;
	gulong use_d_quant;
	gulong clip_d_quant_frame;
	gulong quant_min;
	gulong quant_min_i_vop_under_range;
	gulong quant_max;
	gulong param_changeable;
	gulong changeable_max_bitrate;
	/* mpeg4 */
	gulong out_vos;
	gulong out_gov;
	gulong aspect_ratio_info_type;
	gulong aspect_ratio_info_value;
	gulong vos_profile_level_type;
	gulong vos_profile_level_value;
	gulong out_visual_object_identifier;
	gulong visual_object_verid;
	gulong visual_object_priority;
	gulong video_object_type_indication;
	gulong out_object_layer_identifier;
	gulong video_object_layer_verid;
	gulong video_object_layer_priority;
	gulong error_resilience_mode;
	gulong video_packet_size_mb;
	gulong video_packet_size_bit;
	gulong video_packet_header_extention;
	gulong data_partitioned;
	gulong reversible_vlc;
	gulong high_quality;
	gulong ratecontrol_vbv_skipcheck_enable;
	gulong ratecontrol_vbv_i_vop_noskip;
	gulong ratecontrol_vbv_remain_zero_skip_enable;
	gulong ratecontrol_vbv_buffer_unit_size;
	gulong ratecontrol_vbv_buffer_mode;
	gulong ratecontrol_vbv_max_size;
	gulong ratecontrol_vbv_offset;
	gulong ratecontrol_vbv_offset_rate;
	gulong quant_type;
	gulong use_ac_prediction;
	gulong vop_min_mode;
	gulong vop_min_size;
	gulong intra_thr;
	gulong b_vop_num;
	/* h264 */
	gint ref_frame_num;
	gint output_filler_enable;
	gulong clip_d_quant_next_mb;
	gulong ratecontrol_cpb_skipcheck_enable;
	gulong ratecontrol_cpb_i_vop_noskip;
	gulong ratecontrol_cpb_remain_zero_skip_enable;
	gulong ratecontrol_cpb_buffer_unit_size;
	gulong ratecontrol_cpb_buffer_mode;
	gulong ratecontrol_cpb_max_size;
	gulong ratecontrol_cpb_offset;
	gulong ratecontrol_cpb_offset_rate;
	gulong intra_thr_1;
	gulong intra_thr_2;
	gulong sad_intra_bias;
	gulong regularly_inserted_i_type;
	gulong call_unit;
	gulong use_slice;
	gulong slice_size_mb;
	gulong slice_size_bit;
	gulong slice_type_value_pattern;
	gulong use_mb_partition;
	gulong mb_partition_vector_thr;
	gulong deblocking_mode;
	gulong use_deblocking_filter_control;
	glong deblocking_alpha_offset;
	glong deblocking_beta_offset;	
	gulong me_skip_mode;
	gulong put_start_code;
	gulong seq_param_set_id;
	gulong profile;
	gulong constraint_set_flag;
	gulong level_type;
	gulong level_value;
	gulong out_vui_parameters;
	gulong chroma_qp_index_offset;
	gulong constrained_intra_pred;

};

/**
 * Define Gstreamer SH Video Encoder Class structure
 */
struct _GstSHVideoEncClass
{
	GstElementClass parent;
};

/** 
 * Get gst-sh-mobile-enc object type
 * @return object type
 */
GType gst_sh_video_enc_get_type (void);

/** 
 * Initializes the SH Hardware encoder
 * @param shvideoenc encoder object
 */
void gst_sh_video_enc_init_encoder(GstSHVideoEnc * enc);

/** 
 * Reads the capabilities of the peer element connected to the sink pad
 * @param shvideoenc encoder object
 */
void gst_sh_video_enc_read_sink_caps(GstSHVideoEnc * enc);

/** 
 * Reads the capabilities of the peer element connected to the source pad
 *  @param shvideoenc encoder object
 */
void gst_sh_video_enc_read_src_caps(GstSHVideoEnc * enc);

/** 
 * Sets the capabilities of the source pad
 * @param shvideoenc encoder object
 * @return TRUE if the capabilities could be set, otherwise FALSE
 */
gboolean gst_sh_video_enc_set_src_caps(GstSHVideoEnc * enc);

/** 
 * Launches the encoder in an own thread
 * @param data encoder object
 */
void* gst_sh_video_launch_encoder_thread(void *data);

/** 
 * Sets the properties of the hardware encoder
 * @return TRUE if the properties could be set, otherwise FALSE
 */
gboolean gst_sh_video_enc_set_encoding_properties(GstSHVideoEnc *enc);

G_END_DECLS
#endif
