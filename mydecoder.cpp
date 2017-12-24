//
// Created by xzl on 12/23/17.
//
// cf: hw_decode.c

/*
 * Copyright (c) 2017 Jun Zhao
 * Copyright (c) 2017 Kaixuan Liu
 *
 * HW Acceleration API (video decoding) decode sample
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <zmq.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
}

#include "log.h"
#include "mydecoder.h"

static AVBufferRef *hw_device_ctx = NULL;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = NULL;

static enum AVPixelFormat find_fmt_by_hw_type(const enum AVHWDeviceType type)
{
	enum AVPixelFormat fmt;

	switch (type) {
		case AV_HWDEVICE_TYPE_VAAPI:
			fmt = AV_PIX_FMT_VAAPI;
			break;
		case AV_HWDEVICE_TYPE_DXVA2:
			fmt = AV_PIX_FMT_DXVA2_VLD;
			break;
		case AV_HWDEVICE_TYPE_D3D11VA:
			fmt = AV_PIX_FMT_D3D11;
			break;
		case AV_HWDEVICE_TYPE_VDPAU:
			fmt = AV_PIX_FMT_VDPAU;
			break;
		case AV_HWDEVICE_TYPE_VIDEOTOOLBOX:
			fmt = AV_PIX_FMT_VIDEOTOOLBOX;
			break;
		default:
			fmt = AV_PIX_FMT_NONE;
			break;
	}

	return fmt;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
																				const enum AVPixelFormat *pix_fmts)
{
	const enum AVPixelFormat *p;

	for (p = pix_fmts; *p != -1; p++) {
		if (*p == hw_pix_fmt)
			return *p;
	}

	fprintf(stderr, "Failed to get HW surface format.\n");
	return AV_PIX_FMT_NONE;
}

static int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
	int err = 0;

	if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
																		NULL, NULL, 0)) < 0) {
		fprintf(stderr, "Failed to create specified HW device.\n");
		return err;
	}
	ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

	return err;
}

/* decode a @packet into a set of frames, write each frame as a msg to @sender */
static int decode_write(AVCodecContext *avctx, AVPacket *packet,
												zmq::socket_t & sender)
{
	AVFrame *frame = NULL, *sw_frame = NULL;
	AVFrame *tmp_frame = NULL;
	uint8_t *buffer = NULL;
	int size;
	int ret = 0;

	ret = avcodec_send_packet(avctx, packet);
	if (ret < 0) {
		fprintf(stderr, "Error during decoding\n");
		return ret;
	}

	while (ret >= 0) {
		if (!(frame = av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
			fprintf(stderr, "Can not alloc frame\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		ret = avcodec_receive_frame(avctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			av_frame_free(&frame);
			av_frame_free(&sw_frame);
			return 0;
		} else if (ret < 0) {
			fprintf(stderr, "Error while decoding\n");
			goto fail;
		}

		if (frame->format == hw_pix_fmt) {
			/* retrieve data from GPU to CPU */
			if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
				fprintf(stderr, "Error transferring the data to system memory\n");
				goto fail;
			}
			tmp_frame = sw_frame;
		} else
			tmp_frame = frame;

		size = av_image_get_buffer_size((enum AVPixelFormat) tmp_frame->format,
																		tmp_frame->width,
																		tmp_frame->height, 1);

		buffer = (uint8_t *)av_malloc(size);
		if (!buffer) {
			fprintf(stderr, "Can not alloc buffer\n");
			ret = AVERROR(ENOMEM);
			goto fail;
		}
		ret = av_image_copy_to_buffer(buffer, size,
																	(const uint8_t * const *)tmp_frame->data,
																	(const int *)tmp_frame->linesize,
																	(enum AVPixelFormat) tmp_frame->format,
																	tmp_frame->width, tmp_frame->height, 1);
		if (ret < 0) {
			fprintf(stderr, "Can not copy image to buffer\n");
			goto fail;
		}

		if ((ret = send_one_frame(buffer, size, sender)) < 0) { /* will free buffer */
			fprintf(stderr, "Failed to dump raw data.\n");
			goto fail;
		}

		fail:
		av_frame_free(&frame);
		av_frame_free(&sw_frame);
//		if (buffer)
//			av_freep(&buffer);
		if (ret < 0)
			return ret;
	}

	return 0;
}

//static AVCodec *decoder = NULL;
//static int video_stream = -1;
//static AVCodecContext *decoder_ctx = NULL; /* okay to reuse across files? */

/* decode one file and write frames to @sender */
int decode_one_file(const char *fname, zmq::socket_t & sender) {

	AVFormatContext *input_ctx = NULL;
	int video_stream = -1, ret;
	AVStream *video = NULL;
	AVCodecContext *decoder_ctx = NULL;
	AVCodec *decoder = NULL;
	AVPacket packet;
	enum AVHWDeviceType type;

	const char * devname = "cuda";

	av_register_all();

	type = av_hwdevice_find_type_by_name(devname);
	hw_pix_fmt = find_fmt_by_hw_type(type);
	if (hw_pix_fmt == -1) {
		fprintf(stderr, "Cannot support '%s' in this example.\n", devname);
		return -1;
	}

	/* open the input file */
	xzl_assert(fname);
	ret = avformat_open_input(&input_ctx, fname, NULL, NULL);
	xzl_bug_on_msg(ret != 0, "Cannot find input stream information");

//	once = !decoder;

//	if (once) {
//	xzl_bug_on(video_stream != -1); /* must be invalid */

	ret = avformat_find_stream_info(input_ctx, NULL);
	xzl_bug_on_msg(ret < 0, "Cannot find a video stream in the input file");

	/* find the video stream information */
	ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0) {
		fprintf(stderr, "Cannot find a video stream in the input file\n");
		return -1;
	}
	video_stream = ret;

	decoder_ctx = avcodec_alloc_context3(decoder);
	xzl_bug_on_msg(!decoder_ctx, "no mem for avcodec ctx");
//}

	video = input_ctx->streams[video_stream];

//	if (once) {
		/* fill in the decoder ctx with stream's codec parameters... */
		ret = avcodec_parameters_to_context(decoder_ctx, video->codecpar);
		xzl_bug_on(ret < 0);

		decoder_ctx->get_format  = get_hw_format;
		av_opt_set_int(decoder_ctx, "refcounted_frames", 1, 0);

		ret = hw_decoder_init(decoder_ctx, type);
		xzl_bug_on_msg(ret < 0, "hw decoder init failed");

		ret = avcodec_open2(decoder_ctx, decoder, NULL);
		xzl_bug_on_msg(ret < 0, "Failed to open codec for stream");
//	}

	/* actual decoding and dump the raw data */
	/* actual decoding and dump the raw data */
	while (ret >= 0) {
		if ((ret = av_read_frame(input_ctx, &packet)) < 0)
			break;

		if (video_stream == packet.stream_index)
			ret = decode_write(decoder_ctx, &packet, sender);

		av_packet_unref(&packet);
	}

	/* flush the decoder */
	packet.data = NULL;
	packet.size = 0;
	ret = decode_write(decoder_ctx, &packet, sender);
	av_packet_unref(&packet);

	avcodec_free_context(&decoder_ctx);
	avformat_close_input(&input_ctx);
	av_buffer_unref(&hw_device_ctx);

}
