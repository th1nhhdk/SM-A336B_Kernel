/*
 * drivers/media/platform/exynos/mfc/mfc_buf.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <soc/samsung/exynos-smc.h>
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
#include <soc/samsung/imgloader.h>
#endif
#include <linux/firmware.h>
#include <linux/iommu.h>

#include "mfc_buf.h"

#include "mfc_mem.h"

static int __mfc_alloc_common_context(struct mfc_core *core,
			enum mfc_buf_usage_type buf_type)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_special_buf *ctx_buf;
	struct mfc_ctx_buf_size *buf_size;

	mfc_core_debug_enter();

	ctx_buf = &core->common_ctx_buf;
	ctx_buf->buftype = MFCBUF_NORMAL;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	if (buf_type == MFCBUF_DRM) {
		ctx_buf = &core->drm_common_ctx_buf;
		ctx_buf->buftype = MFCBUF_DRM;
	}
#endif

	buf_size = dev->variant->buf_size->ctx_buf;
	ctx_buf->size = buf_size->dev_ctx;

	if (mfc_mem_special_buf_alloc(dev, ctx_buf)) {
		mfc_core_err("Allocating %s context buffer failed\n",
				buf_type == MFCBUF_DRM ? "secure" : "normal");
		return -ENOMEM;
	}

	mfc_core_debug(2, "[MEMINFO] %s common ctx buf size: %ld, daddr: 0x%08llx\n",
			buf_type == MFCBUF_DRM ? "secure" : "normal",
			ctx_buf->size, ctx_buf->daddr);

	mfc_core_debug_leave();

	return 0;
}

/* Wrapper : allocate context buffers for SYS_INIT */
int mfc_alloc_common_context(struct mfc_core *core)
{
	int ret = 0;

	ret = __mfc_alloc_common_context(core, MFCBUF_NORMAL);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	if (core->fw.drm_status) {
		ret = __mfc_alloc_common_context(core, MFCBUF_DRM);
		if (ret)
			return ret;
	}
#endif

	return ret;
}

/* Release context buffers for SYS_INIT */
static void __mfc_release_common_context(struct mfc_core *core,
					enum mfc_buf_usage_type buf_type)
{
	struct mfc_special_buf *ctx_buf;

	ctx_buf = &core->common_ctx_buf;
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	if (buf_type == MFCBUF_DRM)
		ctx_buf = &core->drm_common_ctx_buf;
#endif

	mfc_mem_special_buf_free(ctx_buf);
	mfc_core_debug(2, "[MEMINFO] Release %s common context buffer\n",
			buf_type == MFCBUF_DRM ? "secure" : "normal");

	ctx_buf->dma_buf = NULL;
	ctx_buf->vaddr = NULL;
	ctx_buf->daddr = 0;
}

/* Release context buffers for SYS_INIT */
void mfc_release_common_context(struct mfc_core *core)
{
	__mfc_release_common_context(core, MFCBUF_NORMAL);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	__mfc_release_common_context(core, MFCBUF_DRM);
#endif
}

/* Allocate memory for instance data buffer */
int mfc_alloc_instance_context(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dev *dev = ctx->dev;
	struct mfc_ctx_buf_size *buf_size;

	mfc_debug_enter();

	buf_size = dev->variant->buf_size->ctx_buf;

	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_DEC:
	case MFC_REG_CODEC_H264_MVC_DEC:
	case MFC_REG_CODEC_HEVC_DEC:
	case MFC_REG_CODEC_BPG_DEC:
		core_ctx->instance_ctx_buf.size = buf_size->h264_dec_ctx;
		break;
	case MFC_REG_CODEC_MPEG4_DEC:
	case MFC_REG_CODEC_H263_DEC:
	case MFC_REG_CODEC_VC1_RCV_DEC:
	case MFC_REG_CODEC_VC1_DEC:
	case MFC_REG_CODEC_MPEG2_DEC:
	case MFC_REG_CODEC_VP8_DEC:
	case MFC_REG_CODEC_VP9_DEC:
	case MFC_REG_CODEC_FIMV1_DEC:
	case MFC_REG_CODEC_FIMV2_DEC:
	case MFC_REG_CODEC_FIMV3_DEC:
	case MFC_REG_CODEC_FIMV4_DEC:
		core_ctx->instance_ctx_buf.size = buf_size->other_dec_ctx;
		break;
	case MFC_REG_CODEC_H264_ENC:
	case MFC_REG_CODEC_AV1_DEC:
		core_ctx->instance_ctx_buf.size = buf_size->h264_enc_ctx;
		break;
	case MFC_REG_CODEC_HEVC_ENC:
	case MFC_REG_CODEC_BPG_ENC:
		core_ctx->instance_ctx_buf.size = buf_size->hevc_enc_ctx;
		break;
	case MFC_REG_CODEC_MPEG4_ENC:
	case MFC_REG_CODEC_H263_ENC:
	case MFC_REG_CODEC_VP8_ENC:
	case MFC_REG_CODEC_VP9_ENC:
		core_ctx->instance_ctx_buf.size = buf_size->other_enc_ctx;
		break;
	default:
		core_ctx->instance_ctx_buf.size = 0;
		mfc_err("Codec type(%d) should be checked!\n",
				ctx->codec_mode);
		return -ENOMEM;
	}

	if (ctx->is_drm)
		core_ctx->instance_ctx_buf.buftype = MFCBUF_DRM;
	else
		core_ctx->instance_ctx_buf.buftype = MFCBUF_NORMAL;

	if (mfc_mem_special_buf_alloc(dev, &core_ctx->instance_ctx_buf)) {
		mfc_err("Allocating context buffer failed\n");
		return -ENOMEM;
	}

	mfc_debug(2, "[MEMINFO] Instance buf ctx[%d] size: %ld, daddr: 0x%08llx\n",
			core_ctx->num, core_ctx->instance_ctx_buf.size,
			core_ctx->instance_ctx_buf.daddr);
	mfc_debug_leave();

	return 0;
}

/* Release instance buffer */
void mfc_release_instance_context(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;

	mfc_core_debug_enter();

	mfc_mem_special_buf_free(&core_ctx->instance_ctx_buf);
	mfc_core_debug(2, "[MEMINFO] Release the instance buffer ctx[%d]\n",
			core_ctx->num);

	mfc_core_debug_leave();
}

static void __mfc_dec_calc_codec_buffer_size(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_dec *dec = ctx->dec_priv;

	/* Codecs have different memory requirements */
	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_DEC:
	case MFC_REG_CODEC_H264_MVC_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		ctx->mv_buf.size = dec->mv_count * ctx->mv_size;
		break;
	case MFC_REG_CODEC_MPEG4_DEC:
	case MFC_REG_CODEC_FIMV1_DEC:
	case MFC_REG_CODEC_FIMV2_DEC:
	case MFC_REG_CODEC_FIMV3_DEC:
	case MFC_REG_CODEC_FIMV4_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		if (dec->loop_filter_mpeg4) {
			ctx->loopfilter_luma_size = ALIGN(ctx->raw_buf.plane_size[0], 256);
			ctx->loopfilter_chroma_size = ALIGN(ctx->raw_buf.plane_size[1] +
							ctx->raw_buf.plane_size[2], 256);
			core_ctx->codec_buf.size = ctx->scratch_buf_size +
				(NUM_MPEG4_LF_BUF * (ctx->loopfilter_luma_size +
						     ctx->loopfilter_chroma_size));
		} else {
			core_ctx->codec_buf.size = ctx->scratch_buf_size;
		}
		break;
	case MFC_REG_CODEC_VC1_RCV_DEC:
	case MFC_REG_CODEC_VC1_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_MPEG2_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_H263_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_VP8_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		break;
	case MFC_REG_CODEC_VP9_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size +
			DEC_STATIC_BUFFER_SIZE;
		break;
	case MFC_REG_CODEC_AV1_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size +
			DEC_AV1_STATIC_BUFFER_SIZE(ctx->img_width, ctx->img_height);
		ctx->mv_buf.size = dec->mv_count * ctx->mv_size;
		break;
	case MFC_REG_CODEC_HEVC_DEC:
	case MFC_REG_CODEC_BPG_DEC:
		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size = ctx->scratch_buf_size;
		ctx->mv_buf.size = dec->mv_count * ctx->mv_size;
		break;
	default:
		core_ctx->codec_buf.size = 0;
		mfc_err("invalid codec type: %d\n", ctx->codec_mode);
		break;
	}

	mfc_debug(2, "[MEMINFO] scratch: %zu, MV: %zu x count %d\n",
			ctx->scratch_buf_size, ctx->mv_size, dec->mv_count);
	if (dec->loop_filter_mpeg4)
		mfc_debug(2, "[MEMINFO] (loopfilter luma: %zu, chroma: %zu) x count %d\n",
				ctx->loopfilter_luma_size, ctx->loopfilter_chroma_size,
				NUM_MPEG4_LF_BUF);
}

static void __mfc_enc_calc_codec_buffer_size(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_enc *enc;
	unsigned int mb_width, mb_height;
	unsigned int lcu_width = 0, lcu_height = 0;

	enc = ctx->enc_priv;
	enc->tmv_buffer_size = 0;

	mb_width = WIDTH_MB(ctx->crop_width);
	mb_height = HEIGHT_MB(ctx->crop_height);

	lcu_width = ENC_LCU_WIDTH(ctx->crop_width);
	lcu_height = ENC_LCU_HEIGHT(ctx->crop_height);

	if (IS_SBWC_DPB(ctx) && ctx->is_10bit) {
		enc->luma_dpb_size = ENC_SBWC_LUMA_10B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
		enc->chroma_dpb_size = ENC_SBWC_CHROMA_10B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
	} else if (IS_SBWC_DPB(ctx) && !ctx->is_10bit) {
		enc->luma_dpb_size = ENC_SBWC_LUMA_8B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
		enc->chroma_dpb_size = ENC_SBWC_CHROMA_8B_DPB_SIZE(ctx->crop_width, ctx->crop_height);
	} else {
		/* default recon buffer size, it can be changed in case of 422, 10bit */
		enc->luma_dpb_size =
			ALIGN(ENC_LUMA_DPB_SIZE(ctx->crop_width, ctx->crop_height), 64);
		enc->chroma_dpb_size =
			ALIGN(ENC_CHROMA_DPB_SIZE(ctx->crop_width, ctx->crop_height), 64);
	}

	/* Codecs have different memory requirements */
	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_ENC:
		enc->me_buffer_size =
			ALIGN(ENC_V100_H264_ME_SIZE(mb_width, mb_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_MPEG4_ENC:
	case MFC_REG_CODEC_H263_ENC:
		enc->me_buffer_size =
			ALIGN(ENC_V100_MPEG4_ME_SIZE(mb_width, mb_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_VP8_ENC:
		enc->me_buffer_size =
			ALIGN(ENC_V100_VP8_ME_SIZE(mb_width, mb_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
			enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_VP9_ENC:
		if (!IS_SBWC_DPB(ctx) && (ctx->is_10bit || ctx->is_422)) {
			enc->luma_dpb_size =
				ALIGN(ENC_VP9_LUMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			enc->chroma_dpb_size =
				ALIGN(ENC_VP9_CHROMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			mfc_debug(2, "[10BIT] VP9 10bit or 422 recon luma size: %zu chroma size: %zu\n",
					enc->luma_dpb_size, enc->chroma_dpb_size);
		}
		enc->me_buffer_size =
			ALIGN(ENC_V100_VP9_ME_SIZE(lcu_width, lcu_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
					   enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	case MFC_REG_CODEC_HEVC_ENC:
	case MFC_REG_CODEC_BPG_ENC:
		if (!IS_SBWC_DPB(ctx) && (ctx->is_10bit || ctx->is_422)) {
			enc->luma_dpb_size =
				ALIGN(ENC_HEVC_LUMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			enc->chroma_dpb_size =
				ALIGN(ENC_HEVC_CHROMA_DPB_10B_SIZE(ctx->crop_width, ctx->crop_height), 64);
			mfc_debug(2, "[10BIT] HEVC 10bit or 422 recon luma size: %zu chroma size: %zu\n",
					enc->luma_dpb_size, enc->chroma_dpb_size);
		}
		enc->me_buffer_size =
			ALIGN(ENC_V100_HEVC_ME_SIZE(lcu_width, lcu_height), 256);

		ctx->scratch_buf_size = ALIGN(ctx->scratch_buf_size, 256);
		core_ctx->codec_buf.size =
			ctx->scratch_buf_size + enc->tmv_buffer_size +
			(ctx->dpb_count * (enc->luma_dpb_size +
					   enc->chroma_dpb_size + enc->me_buffer_size));
		break;
	default:
		core_ctx->codec_buf.size = 0;
		mfc_err("invalid codec type: %d\n", ctx->codec_mode);
		break;
	}

	mfc_debug(2, "[MEMINFO] scratch: %zu, TMV: %zu, (recon luma: %zu, chroma: %zu, me: %zu) x count %d\n",
			ctx->scratch_buf_size, enc->tmv_buffer_size,
			enc->luma_dpb_size, enc->chroma_dpb_size, enc->me_buffer_size,
			ctx->dpb_count);
}

/* Allocate codec buffers */
int mfc_alloc_codec_buffers(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx *ctx = core_ctx->ctx;

	mfc_debug_enter();

	if (ctx->type == MFCINST_DECODER) {
		__mfc_dec_calc_codec_buffer_size(core_ctx);
	} else if (ctx->type == MFCINST_ENCODER) {
		__mfc_enc_calc_codec_buffer_size(core_ctx);
	} else {
		mfc_err("invalid type: %d\n", ctx->type);
		return -EINVAL;
	}

	if (ctx->is_drm) {
		core_ctx->codec_buf.buftype = MFCBUF_DRM;
		ctx->mv_buf.buftype = MFCBUF_DRM;
	} else {
		core_ctx->codec_buf.buftype = MFCBUF_NORMAL;
		ctx->mv_buf.buftype = MFCBUF_NORMAL;
	}

	if (core_ctx->codec_buf.size > 0) {
		if (mfc_mem_special_buf_alloc(dev, &core_ctx->codec_buf)) {
			mfc_err("Allocating codec buffer failed\n");
			return -ENOMEM;
		}
		core_ctx->codec_buffer_allocated = 1;
	} else if (ctx->codec_mode == MFC_REG_CODEC_MPEG2_DEC) {
		core_ctx->codec_buffer_allocated = 1;
	}

	if (!ctx->mv_buffer_allocated && ctx->mv_buf.size > 0) {
		if (mfc_mem_special_buf_alloc(dev, &ctx->mv_buf)) {
			mfc_err("Allocating MV buffer failed\n");
			return -ENOMEM;
		}
		ctx->mv_buffer_allocated = 1;
	}

	mfc_debug(2, "[MEMINFO] Codec buf size: %ld, addr: %#llx\n",
			core_ctx->codec_buf.size, core_ctx->codec_buf.daddr);
	mfc_debug(2, "[MEMINFO] MV buf size: %ld, addr: %#llx\n",
			ctx->mv_buf.size, ctx->mv_buf.daddr);

	mfc_debug_leave();

	return 0;
}

/* Release buffers allocated for codec */
void mfc_release_codec_buffers(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_ctx *ctx = core_ctx->ctx;

	if (core_ctx->codec_buffer_allocated) {
		mfc_mem_special_buf_free(&core_ctx->codec_buf);
		core_ctx->codec_buffer_allocated = 0;
	}

	if (ctx->mv_buffer_allocated) {
		mfc_mem_special_buf_free(&ctx->mv_buf);
		ctx->mv_buffer_allocated = 0;
	}

	mfc_release_scratch_buffer(core_ctx);
	mfc_core_debug(2, "[MEMINFO] Release the codec buffer ctx[%d]\n",
			core_ctx->num);
}

int mfc_alloc_scratch_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx *ctx = core_ctx->ctx;

	mfc_debug_enter();

	if (core_ctx->scratch_buffer_allocated) {
		mfc_mem_special_buf_free(&core_ctx->scratch_buf);
		core_ctx->scratch_buffer_allocated = 0;
		mfc_debug(2, "[MEMINFO] Release the scratch buffer ctx[%d]\n",
				core_ctx->num);
	}

	if (ctx->is_drm)
		core_ctx->scratch_buf.buftype = MFCBUF_DRM;
	else
		core_ctx->scratch_buf.buftype = MFCBUF_NORMAL;

	core_ctx->scratch_buf.size =  ALIGN(ctx->scratch_buf_size, 256);
	if (core_ctx->scratch_buf.size > 0) {
		if (mfc_mem_special_buf_alloc(dev, &core_ctx->scratch_buf)) {
			mfc_err("Allocating scratch_buf buffer failed\n");
			return -ENOMEM;
		}
		core_ctx->scratch_buffer_allocated = 1;
	}

	mfc_debug(2, "[MEMINFO] scratch buf ctx[%d] size: %ld, addr: 0x%08llx\n",
			core_ctx->num, ctx->scratch_buf_size,
			core_ctx->scratch_buf.daddr);

	mfc_debug_leave();

	return 0;
}

void mfc_release_scratch_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;

	mfc_debug_enter();
	if (core_ctx->scratch_buffer_allocated) {
		mfc_mem_special_buf_free(&core_ctx->scratch_buf);
		core_ctx->scratch_buffer_allocated = 0;
		mfc_debug(2, "[MEMINFO] Release the scratch buffer ctx[%d]\n",
				core_ctx->num);
	}
	mfc_debug_leave();
}

/* Allocation buffer of debug infor memory for FW debugging */
int mfc_alloc_dbg_info_buffer(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx_buf_size *buf_size = dev->variant->buf_size->ctx_buf;

	mfc_core_debug(2, "Allocate a debug-info buffer\n");

	core->dbg_info_buf.buftype = MFCBUF_NORMAL;
	core->dbg_info_buf.size = buf_size->dbg_info_buf;
	if (mfc_mem_special_buf_alloc(dev, &core->dbg_info_buf)) {
		mfc_core_err("Allocating debug info buffer failed\n");
		return -ENOMEM;
	}
	mfc_core_debug(2, "[MEMINFO] debug info buf size: %ld, daddr: 0x%08llx, vaddr: 0x%p\n",
			core->dbg_info_buf.size, core->dbg_info_buf.daddr,
			core->dbg_info_buf.vaddr);

	return 0;
}

/* Release buffer of debug infor memory for FW debugging */
void mfc_release_dbg_info_buffer(struct mfc_core *core)
{
	if (!core->dbg_info_buf.dma_buf)
		mfc_core_debug(2, "debug info buffer is already freed\n");

	mfc_mem_special_buf_free(&core->dbg_info_buf);
	mfc_core_debug(2, "[MEMINFO] Release the debug info buffer\n");
}

/* Allocation buffer of ROI macroblock information */
static int __mfc_alloc_enc_roi_buffer(struct mfc_core_ctx *core_ctx,
			size_t size, struct mfc_special_buf *roi_buf)
{
	struct mfc_core *core = core_ctx->core;
	struct mfc_dev *dev = core->dev;

	roi_buf->size = size;
	roi_buf->buftype = MFCBUF_NORMAL;

	if (roi_buf->dma_buf == NULL) {
		if (mfc_mem_special_buf_alloc(dev, roi_buf)) {
			mfc_err("[ROI] Allocating ROI buffer failed\n");
			return -ENOMEM;
		}
	}
	mfc_core_debug(2, "[MEMINFO][ROI] roi buf ctx[%d] size: %ld, daddr: 0x%08llx, vaddr: 0x%p\n",
			core_ctx->num, roi_buf->size, roi_buf->daddr, roi_buf->vaddr);

	memset(roi_buf->vaddr, 0, roi_buf->size);

	return 0;
}

/* Wrapper : allocation ROI buffers */
int mfc_alloc_enc_roi_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_enc *enc = ctx->enc_priv;
	unsigned int mb_width, mb_height;
	unsigned int lcu_width = 0, lcu_height = 0;
	size_t size;
	int i;

	mb_width = WIDTH_MB(ctx->crop_width);
	mb_height = HEIGHT_MB(ctx->crop_height);

	switch (ctx->codec_mode) {
	case MFC_REG_CODEC_H264_ENC:
		size = ((((mb_width * (mb_height + 1) / 2) + 15) / 16) * 16) * 2;
		break;
	case MFC_REG_CODEC_MPEG4_ENC:
	case MFC_REG_CODEC_VP8_ENC:
		size = mb_width * mb_height;
		break;
	case MFC_REG_CODEC_VP9_ENC:
		lcu_width = (ctx->crop_width + 63) / 64;
		lcu_height = (ctx->crop_height + 63) / 64;
		size = lcu_width * lcu_height * 4;
		break;
	case MFC_REG_CODEC_HEVC_ENC:
		lcu_width = (ctx->crop_width + 31) / 32;
		lcu_height = (ctx->crop_height + 31) / 32;
		size = lcu_width * lcu_height;
		break;
	default:
		mfc_debug(2, "ROI not supported codec type(%d). Allocate with default size\n",
				ctx->codec_mode);
		size = mb_width * mb_height;
		break;
	}

	for (i = 0; i < MFC_MAX_EXTRA_BUF; i++) {
		if (__mfc_alloc_enc_roi_buffer(core_ctx, size,
					&enc->roi_buf[i]) < 0) {
			mfc_err("[ROI] Allocating remapping buffer[%d] failed\n",
							i);
			return -ENOMEM;
		}
	}

	return 0;
}

/* Release buffer of ROI macroblock information */
void mfc_release_enc_roi_buffer(struct mfc_core_ctx *core_ctx)
{
	struct mfc_ctx *ctx = core_ctx->ctx;
	struct mfc_enc *enc = ctx->enc_priv;
	int i;

	for (i = 0; i < MFC_MAX_EXTRA_BUF; i++)
		if (enc->roi_buf[i].dma_buf)
			mfc_mem_special_buf_free(&enc->roi_buf[i]);

	mfc_debug(2, "[MEMINFO][ROI] Release the ROI buffer\n");
}

int mfc_otf_alloc_stream_buf(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;
	struct _otf_handle *handle = ctx->otf_handle;
	struct _otf_debug *debug = &handle->otf_debug;
	struct mfc_special_buf *buf;
	struct mfc_raw_info *raw = &ctx->raw_buf;
	int i;

	mfc_debug_enter();

	for (i = 0; i < OTF_MAX_BUF; i++) {
		buf = &debug->stream_buf[i];
		buf->buftype = MFCBUF_NORMAL;
		buf->size = raw->total_plane_size;
		if (mfc_mem_special_buf_alloc(dev, buf)) {
			mfc_ctx_err("[OTF] Allocating stream buffer failed\n");
			return -EINVAL;
		}
		mfc_debug(2, "[OTF][MEMINFO] OTF stream buf[%d] size: %ld, daddr: 0x%08llx, vaddr: 0x%p\n",
				i, buf->size, buf->daddr, buf->vaddr);
		memset(buf->vaddr, 0, raw->total_plane_size);
	}

	mfc_debug_leave();

	return 0;
}

void mfc_otf_release_stream_buf(struct mfc_ctx *ctx)
{
	struct _otf_handle *handle = ctx->otf_handle;
	struct _otf_debug *debug = &handle->otf_debug;
	struct mfc_special_buf *buf;
	int i;

	mfc_debug_enter();

	for (i = 0; i < OTF_MAX_BUF; i++) {
		buf = &debug->stream_buf[i];
		if (buf->dma_buf)
			mfc_mem_special_buf_free(buf);
	}

	mfc_debug(2, "[OTF][MEMINFO] Release the OTF stream buffer\n");
	mfc_debug_leave();
}

int mfc_alloc_metadata_buffer(struct mfc_ctx *ctx)
{
	struct mfc_dev *dev = ctx->dev;

	mfc_debug_enter();

	if (ctx->metadata_buffer_allocated)
		return 0;

	ctx->metadata_buf.size = HDR10_PLUS_DATA_SIZE * MFC_MAX_DPBS;
	if (ctx->is_drm)
		ctx->metadata_buf.buftype = MFCBUF_DRM;
	else
		ctx->metadata_buf.buftype = MFCBUF_NORMAL;

	if (mfc_mem_special_buf_alloc(dev, &ctx->metadata_buf)) {
		mfc_ctx_err("Allocating metadata_buf buffer failed\n");
		return -ENOMEM;
	}
	ctx->metadata_buffer_allocated = 1;

	mfc_debug(2, "[MEMINFO] metadata buf ctx[%d] size: %ld, addr: 0x%08llx\n",
			ctx->num, ctx->metadata_buf.size, ctx->metadata_buf.daddr);

	mfc_debug_leave();

	return 0;
}

void mfc_release_metadata_buffer(struct mfc_ctx *ctx)
{
	mfc_debug_enter();

	if (ctx->metadata_buffer_allocated) {
		mfc_mem_special_buf_free(&ctx->metadata_buf);
		ctx->metadata_buffer_allocated = 0;
		mfc_debug(2, "[MEMINFO] Release the metadata buffer ctx[%d]\n", ctx->num);
	}

	mfc_debug_leave();
}

/* Allocate firmware */
int mfc_alloc_firmware(struct mfc_core *core)
{
	struct mfc_dev *dev = core->dev;
	struct mfc_ctx_buf_size *buf_size;
	struct mfc_special_buf *fw_buf;
#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	unsigned long secure_daddr = 0;
#endif

	mfc_core_debug_enter();

	buf_size = dev->variant->buf_size->ctx_buf;

	if (core->fw_buf.sgt)
		return 0;

	mfc_core_debug(4, "[F/W] Allocating memory for firmware\n");

	core->fw_buf.size = dev->variant->buf_size->firmware_code;
	core->fw_buf.buftype = MFCBUF_NORMAL_FW;
	if (mfc_mem_special_buf_alloc(dev, &core->fw_buf)) {
		mfc_core_err("[F/W] Allocating normal firmware buffer failed\n");
		return -ENOMEM;
	}

	fw_buf = &core->fw_buf;
	if (mfc_iommu_map_firmware(core, fw_buf))
		goto err_reserve_iova;

	mfc_core_info("[MEMINFO][F/W] MFC-%d FW normal: 0x%08llx (vaddr: 0x%08llx, paddr:%#llx), size: %08zu\n",
			core->id, core->fw_buf.daddr, core->fw_buf.vaddr, core->fw_buf.paddr,
			core->fw_buf.size);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	core->drm_fw_buf.buftype = MFCBUF_DRM_FW;
	core->drm_fw_buf.size = core->fw_buf.size;
	if (mfc_mem_special_buf_alloc(dev, &core->drm_fw_buf)) {
		mfc_core_err("[F/W] Allocating DRM firmware buffer failed\n");
		goto err_reserve_iova;
	}

	/* allocate Secure-DVA region */
	secure_daddr = secure_iova_alloc(core->drm_fw_buf.size, EXYNOS_SECBUF_PROT_ALIGNMENTS);
	if (!secure_daddr) {
		mfc_core_err("DRM F/W buffer can not get IOVA!\n");
		goto err_reserve_iova_secure;
	}

	core->drm_fw_buf.daddr = (dma_addr_t)secure_daddr;

	mfc_core_info("[MEMINFO][F/W] MFC-%d FW DRM: 0x%08llx (vaddr: 0x%08llx, paddr:%#llx), size: %08zu\n",
			core->id, core->drm_fw_buf.daddr,
			core->drm_fw_buf.vaddr, core->drm_fw_buf.paddr,
			core->drm_fw_buf.size);
#endif

	mfc_core_debug_leave();

	return 0;

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
err_reserve_iova_secure:
	mfc_mem_special_buf_free(&core->drm_fw_buf);
#endif
err_reserve_iova:
	iommu_unmap(core->domain, fw_buf->daddr, fw_buf->map_size);
	mfc_mem_special_buf_free(&core->fw_buf);
	return -ENOMEM;
}

/* Load firmware to MFC */
int mfc_load_firmware(struct mfc_core *core, const u8 *fw_data, size_t fw_size)
{
	mfc_core_debug(2, "[MEMINFO][F/W] loaded F/W Size: %zu\n", fw_size);

	if (fw_size > core->fw_buf.size) {
		mfc_core_err("[MEMINFO][F/W] MFC firmware(%zu) is too big to be loaded in memory(%zu)\n",
				fw_size, core->fw_buf.size);
		return -ENOMEM;
	}

	core->fw.fw_size = fw_size;

	if (core->fw_buf.sgt == NULL || core->fw_buf.daddr == 0) {
		mfc_core_err("[F/W] MFC firmware is not allocated or was not mapped correctly\n");
		return -EINVAL;
	}

	/*  This adds to clear with '0' for firmware memory except code region. */
	mfc_core_debug(4, "[F/W] memset before memcpy for normal fw\n");
	memset((core->fw_buf.vaddr + fw_size), 0, (core->fw_buf.size - fw_size));
	memcpy(core->fw_buf.vaddr, fw_data, fw_size);

	/* cache flush for memcpy by CPU */
	dma_sync_sgtable_for_device(core->device, core->fw_buf.sgt, DMA_TO_DEVICE);

	if (core->drm_fw_buf.vaddr) {
		mfc_core_debug(4, "[F/W] memset before memcpy for secure fw\n");
		memset((core->drm_fw_buf.vaddr + fw_size), 0, (core->drm_fw_buf.size - fw_size));
		memcpy(core->drm_fw_buf.vaddr, fw_data, fw_size);
		mfc_core_debug(4, "[F/W] copy firmware to secure region\n");

		/* cache flush for memcpy by CPU */
		dma_sync_sgtable_for_device(core->device, core->drm_fw_buf.sgt, DMA_TO_DEVICE);
		mfc_core_debug(4, "[F/W] cache flush for secure region\n");
	}

	return 0;
}

/* Request and load firmware to MFC */
int mfc_request_load_firmware(struct mfc_core *core)
{
#if !IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	const struct firmware *fw_blob;
#endif
	int ret;

	mfc_core_debug_enter();
#if IS_ENABLED(CONFIG_EXYNOS_IMGLOADER)
	mfc_core_debug(4, "[F/W] Requesting imgloader boot for F/W\n");

	ret = imgloader_boot(&core->mfc_imgloader_desc);
	if (ret) {
		mfc_core_err("[F/W] imgloader boot failed.\n");
		return -EINVAL;
	}
#else
	mfc_core_debug(4, "[F/W] Requesting F/W\n");
	ret = request_firmware(&fw_blob, MFC_FW_NAME, core->dev->v4l2_dev.dev);

	if (ret != 0) {
		mfc_core_err("[F/W] Couldn't find the F/W invalid path\n");
		return ret;
	}

	ret = mfc_load_firmware(core, fw_blob->data, fw_blob->size);
	if (ret) {
		mfc_core_err("[F/W] Failed to load the MFC F/W\n");
		release_firmware(fw_blob);
		return ret;
	}

	release_firmware(fw_blob);
#endif
	mfc_core_debug_leave();

	return 0;
}

/* Release firmware memory */
int mfc_release_firmware(struct mfc_core *core)
{
	struct mfc_special_buf *fw_buf;

	/* Before calling this function one has to make sure
	 * that MFC is no longer processing */
	fw_buf = &core->fw_buf;
	if (!fw_buf->dma_buf) {
		mfc_core_err("[F/W] firmware memory is already freed\n");
		return -EINVAL;
	}
	iommu_unmap(core->domain, fw_buf->daddr, fw_buf->map_size);

#if IS_ENABLED(CONFIG_EXYNOS_CONTENT_PATH_PROTECTION)
	/* free Secure-DVA region */
	if (core->drm_fw_buf.daddr)
		secure_iova_free(core->drm_fw_buf.daddr, core->drm_fw_buf.size);
	core->drm_fw_buf.daddr = 0;
	mfc_mem_special_buf_free(&core->drm_fw_buf);
#endif

	mfc_mem_special_buf_free(&core->fw_buf);

	return 0;
}
