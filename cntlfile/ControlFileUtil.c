/*
 * libshcodecs: A library for controlling SH-Mobile hardware codecs
 * Copyright (C) 2009 Renesas Technology Corp.
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>		/* 050523 */

#include "avcbencsmp.h"

#include <shcodecs/shcodecs_encoder.h>

static int ReadUntilKeyMatch(FILE * fp_in, const char *key_word, char *buf_value)
{
	char buf_line[256], buf_work_value[256], *pos;
	int line_length, keyword_length, try_count;

	keyword_length = strlen(key_word);

	try_count = 1;

      retry:;
	while (fgets(buf_line, 256, fp_in)) {
		line_length = strlen(buf_line);
		if (line_length < keyword_length) {
			continue;
		}

		if (strncmp(key_word, &buf_line[0], keyword_length) == 0) {
			pos = strchr(&buf_line[keyword_length], '=');
			if (pos == NULL) {
				return (-2);
				/* キーワードに一致する行は見つかったが、"="が見つからなかった */
				;
			}
			strcpy(buf_work_value, (pos + 2));
			pos = strchr(&buf_work_value[1], ';');
			if (pos == NULL) {
				return (-3);
				/* キーワードに一致する行は見つかったが、";"が見つからなかった */
				;
			} else {
				*pos = '\0';
			}

			strcpy(buf_value, buf_work_value);
			return (1);	/* 見つかった */
		}
	}

	/* 見つからなかったときは、ファイルの先頭に戻る */
	if (try_count == 1) {
		rewind(fp_in);
		try_count = 2;
		goto retry;
	} else {
		return (-1);	/* 見つからなった */
	}
}

/*****************************************************************************
 * Function Name	: GetValueFromCtrlFile
 * Description		: コントロールファイルから、キーワードに対する数値を読み込み、戻り値で返す
 *					
 * Parameters		: 
 * Called functions	: 		  
 * Global Data		: 
 * Return Value		: 
 *****************************************************************************/
static long GetValueFromCtrlFile(FILE * fp_in, const char *key_word,
			  int *status_flag)
{
	char buf_line[256];
	long return_code, work_value;

	*status_flag = 1;	/* 正常のとき */

	if ((fp_in == NULL) || (key_word == NULL)) {
		*status_flag = -1;	/* 引数エラーのとき */
		return (0);
	}

	return_code = ReadUntilKeyMatch(fp_in, key_word, &buf_line[0]);
	if (return_code == 1) {
		*status_flag = 1;	/* 正常のとき */
		work_value = atoi((const char *) &buf_line[0]);
	} else {
		*status_flag = -1;	/* 見つからなかった等のエラーのとき */
		work_value = 0;
	}

	return (work_value);
}

/*****************************************************************************
 * Function Name	: GetFromCtrlFtoEncoding_property
 * Description		: コントロールファイルから、構造体avcbe_encoding_propertyのメンバ値を読み込み、引数に設定して返す
 *					
 * Parameters		: 
 * Called functions	: 		  
 * Global Data		: 
 * Return Value		: 
 *****************************************************************************/
static int GetFromCtrlFtoEncoding_property(FILE * fp_in,
				    SHCodecs_Encoder * encoder)
{
	int status_flag;
	long return_value;

	return_value =
	    GetValueFromCtrlFile(fp_in, "bitrate", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_bitrate (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "I_vop_interval", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_I_vop_interval (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "mv_mode", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mv_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "fcode_forward", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_fcode_forward (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "search_mode", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_search_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "search_time_fixed", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_search_time_fixed (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_skip_enable",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_ratecontrol_skip_enable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_use_prevquant",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_ratecontrol_use_prevquant (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_respect_type ", &status_flag);	/* 050426 */
	if (status_flag == 1) {
		shcodecs_encoder_set_ratecontrol_respect_type (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_intra_thr_changeable",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_ratecontrol_intra_thr_changeable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "control_bitrate_length",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_control_bitrate_length (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "intra_macroblock_refresh_cycle",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_intra_macroblock_refresh_cycle (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_format", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_video_format (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "frame_num_resolution",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_frame_num_resolution (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "noise_reduction", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_noise_reduction (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "reaction_param_coeff",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_reaction_param_coeff (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "weightedQ_mode", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_weightedQ_mode (encoder, return_value);
	}

	return (1);		/* 正常終了 */
}

/*****************************************************************************
 * Function Name	: GetFromCtrlFtoOther_options_H264
 * Description		: コントロールファイルから、構造体avcbe_other_options_h264のメンバ値を読み込み、引数に設定して返す
 *					
 * Parameters		: 
 * Called functions	: 		  
 * Global Data		: 
 * Return Value		: 
 *****************************************************************************/
static int GetFromCtrlFtoOther_options_H264(FILE * fp_in,
				     SHCodecs_Encoder * encoder)
{
	int status_flag;
	long return_value;

	return_value =
	    GetValueFromCtrlFile(fp_in, "Ivop_quant_initial_value",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_Ivop_quant_initial_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "Pvop_quant_initial_value",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_Pvop_quant_initial_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "use_dquant", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_use_dquant (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "clip_dquant_next_mb",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_clip_dquant_next_mb (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "clip_dquant_frame", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_clip_dquant_frame (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "quant_min", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_quant_min (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "quant_min_Ivop_under_range", &status_flag);	/* 050509 */
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_quant_min_Ivop_under_range (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "quant_max", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_quant_max (encoder, return_value);
	}


	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_skipcheck_enable ", &status_flag);	/* 050524 */
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_skipcheck_enable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_Ivop_noskip",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_Ivop_noskip (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_remain_zero_skip_enable", &status_flag);	/* 050524 */
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_remain_zero_skip_enable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_offset",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_offset (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_offset_rate",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_offset_rate (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_buffer_mode",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_buffer_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_max_size",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_max_size (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_cpb_buffer_unit_size",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_ratecontrol_cpb_buffer_unit_size (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "intra_thr_1", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_intra_thr_1 (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "intra_thr_2", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_intra_thr_2 (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "sad_intra_bias", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_sad_intra_bias (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "regularly_inserted_I_type",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_regularly_inserted_I_type (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "call_unit", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_call_unit (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "use_slice", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_use_slice (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "slice_size_mb", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_slice_size_mb (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "slice_size_bit", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_slice_size_bit (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "slice_type_value_pattern",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_slice_type_value_pattern (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "use_mb_partition", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_use_mb_partition (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "mb_partition_vector_thr",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_mb_partition_vector_thr (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "deblocking_mode", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_deblocking_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "use_deblocking_filter_control",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_use_deblocking_filter_control (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "deblocking_alpha_offset",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_deblocking_alpha_offset (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "deblocking_beta_offset",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_deblocking_beta_offset (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "me_skip_mode", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_me_skip_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "put_start_code", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_put_start_code (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "param_changeable", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_param_changeable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "changeable_max_bitrate",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_changeable_max_bitrate (encoder, return_value);
	}

	/* SequenceHeaderParameter */
	return_value =
	    GetValueFromCtrlFile(fp_in, "seq_param_set_id", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_seq_param_set_id (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "profile", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_profile (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "constraint_set_flag",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_constraint_set_flag (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "level_type", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_level_type (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "level_value", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_level_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "out_vui_parameters",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_out_vui_parameters (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "chroma_qp_index_offset",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_chroma_qp_index_offset (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "constrained_intra_pred",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_h264_constrained_intra_pred (encoder, return_value);
	}

	return (1);		/* 正常終了 */
}

/*****************************************************************************
 * Function Name	: GetFromCtrlFtoOther_options_MPEG4
 * Description		: コントロールファイルから、構造体avcbe_other_options_mpeg4のメンバ値を読み込み、引数に設定して返す
 *					
 * Parameters		: 
 * Called functions	: 		  
 * Global Data		: 
 * Return Value		: 
 *****************************************************************************/
static int GetFromCtrlFtoOther_options_MPEG4(FILE * fp_in,
					SHCodecs_Encoder * encoder)
{
	int status_flag;
	long return_value;

	return_value =
	    GetValueFromCtrlFile(fp_in, "out_vos", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_out_vos (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "out_gov", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_out_gov (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "aspect_ratio_info_type",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_aspect_ratio_info_type (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "aspect_ratio_info_value",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_aspect_ratio_info_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "vos_profile_level_type",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_vos_profile_level_type (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "vos_profile_level_value",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_vos_profile_level_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "out_visual_object_identifier",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_out_visual_object_identifier (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "visual_object_verid",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_visual_object_verid (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "visual_object_priority",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_visual_object_priority (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_object_type_indication",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_video_object_type_indication (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "out_object_layer_identifier",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_out_object_layer_identifier (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_object_layer_verid",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_video_object_layer_verid (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_object_layer_priority",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_video_object_layer_priority (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "error_resilience_mode",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_error_resilience_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_packet_size_mb",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_video_packet_size_mb (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_packet_size_bit",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_video_packet_size_bit (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "video_packet_header_extention",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_video_packet_header_extention (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "data_partitioned", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_data_partitioned (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "reversible_vlc", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_reversible_vlc (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "high_quality", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_high_quality (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "param_changeable", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_param_changeable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "changeable_max_bitrate",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_changeable_max_bitrate (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "Ivop_quant_initial_value",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_Ivop_quant_initial_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "Pvop_quant_initial_value",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_Pvop_quant_initial_value (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "use_dquant", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_use_dquant (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "clip_dquant_frame", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_clip_dquant_frame (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "quant_min", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_quant_min (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "quant_min_Ivop_under_range", &status_flag);	/* 050509 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_quant_min_Ivop_under_range (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "quant_max", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_quant_max (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_skipcheck_enable",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_skipcheck_enable (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_Ivop_noskip",
				 &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_Ivop_noskip (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_remain_zero_skip_enable", &status_flag);	/* 050524 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_remain_zero_skip_enable (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_buffer_unit_size", &status_flag);	/* 順序変更 050601 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_buffer_unit_size (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_buffer_mode", &status_flag);	/* 順序変更 050601 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_buffer_mode (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_max_size", &status_flag);	/* 順序変更 050601 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_max_size (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_offset", &status_flag);	/* 順序変更 050601 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_offset (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "rate_ctrl_vbv_offset_rate", &status_flag);	/* 順序変更 050601 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_ratecontrol_vbv_offset_rate (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "quant_type", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_quant_type (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "use_AC_prediction", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_use_AC_prediction (encoder, return_value);
	}

	return_value = GetValueFromCtrlFile(fp_in, "vop_min_mode", &status_flag);	/* 050524 */
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_vop_min_mode (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "vop_min_size", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_vop_min_size (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "intra_thr", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_intra_thr (encoder, return_value);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "b_vop_num", &status_flag);
	if (status_flag == 1) {
		shcodecs_encoder_set_mpeg4_b_vop_num (encoder, return_value);
	}

	return (1);		/* 正常終了 */
}


/*****************************************************************************
 * Function Name	: GetFromCtrlFTop
 * Description		: コントロールファイルから、入力ファイル、出力先、ストリームタイプを得る
 * Parameters		: 省略
 * Called functions	: 		  
 * Global Data		: 
 * Return Value		: 1: 正常終了、-1: エラー
 *****************************************************************************/
int GetFromCtrlFTop(const char *control_filepath,
		    APPLI_INFO * appli_info, long *stream_type)
{
	FILE *fp_in;
	int status_flag;
	long return_value;

	if ((control_filepath == NULL) ||
	    (appli_info == NULL) || (stream_type == NULL)) {
		return (-1);
	}

	fp_in = fopen(control_filepath, "rt");
	if (fp_in == NULL) {
		return (-1);
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "stream_type", &status_flag);
	if (status_flag == 1) {
		*stream_type = return_value;
	}
	return_value =
	    GetValueFromCtrlFile(fp_in, "x_pic_size", &status_flag);
	if (status_flag == 1) {
		appli_info->xpic = return_value;
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "y_pic_size", &status_flag);
	if (status_flag == 1) {
		appli_info->ypic = return_value;
	}

	return_value =
	    GetValueFromCtrlFile(fp_in, "frame_rate", &status_flag);
	if (status_flag == 1) {
		appli_info->frame_rate = return_value;
	}

	fclose(fp_in);

	return (1);		/* 正常終了 */

}

/*****************************************************************************
 * Function Name	: GetFromCtrlFtoEncParam
 * Description		: コントロールファイルから、構造体avcbe_encoding_property、avcbe_other_options_h264、
 *　　　　　　　　　 avcbe_other_options_mpeg4等のメンバ値を読み込み、設定して返す
 * Parameters		: 省略
 * Called functions	: 		  
 * Global Data		: 
 * Return Value		: 1: 正常終了、-1: エラー
 *****************************************************************************/
int GetFromCtrlFtoEncParam(SHCodecs_Encoder * encoder,
                           APPLI_INFO * appli_info)
{
	FILE *fp_in;
	int status_flag;
	long return_value;
	long stream_type;

	if ((encoder == NULL) ||
            (appli_info == NULL) ||
	    (appli_info->ctrl_file_name_buf == NULL)) {
		return (-1);
	}

	fp_in = fopen(appli_info->ctrl_file_name_buf, "rt");
	if (fp_in == NULL) {
		return (-1);
	}

	/*** avcbe_encoding_property ***/
	GetFromCtrlFtoEncoding_property(fp_in, encoder);

        stream_type = shcodecs_encoder_get_stream_type (encoder);

	if (stream_type == SHCodecs_Format_H264) {
		/*** avcbe_other_options_h264 ***/
		GetFromCtrlFtoOther_options_H264(fp_in, encoder);
	        return_value = GetValueFromCtrlFile(fp_in, "ref_frame_num", &status_flag);
	        if (status_flag == 1) {
		        shcodecs_encoder_set_ref_frame_num (encoder, return_value);
	        }
	        return_value = GetValueFromCtrlFile(fp_in, "filler_output_on", &status_flag);
	        if (status_flag == 1) {
	        	shcodecs_encoder_set_output_filler_enable (encoder, return_value);
	        }
	} else {
		/*** avcbe_other_options_mpeg4 ***/
		GetFromCtrlFtoOther_options_MPEG4(fp_in, encoder);
	}

	fclose(fp_in);

	return (1);		/* 正常終了 */
}

