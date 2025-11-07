#include "star6.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static i6_common_pixfmt plane_pixel_format(const i6_snr_plane *plane) {
    if (plane->bayer >= I6_BAYER_END)
        return plane->pixFmt;
    return (i6_common_pixfmt)(I6_PIXFMT_RGB_BAYER + plane->precision * I6_BAYER_END + plane->bayer);
}

int star6_context_init(star6_context *ctx) {
    if (!ctx)
        return EXIT_FAILURE;

    memset(ctx, 0, sizeof(*ctx));
    ctx->sensor_index = -1;
    ctx->vif_dev = 0;
    ctx->vif_chn = 0;
    ctx->vif_port = 0;
    ctx->vpe_dev = 0;
    ctx->vpe_chn = 0;
    ctx->vpe_port = 0;
    ctx->venc_chn = 0;

    if (i6_sys_load(&ctx->sys))
        return EXIT_FAILURE;
    if (ctx->sys.fnInit())
        return EXIT_FAILURE;

    if (i6_snr_load(&ctx->snr))
        return EXIT_FAILURE;
    if (i6_vif_load(&ctx->vif))
        return EXIT_FAILURE;
    if (i6_vpe_load(&ctx->vpe))
        return EXIT_FAILURE;
    if (i6_venc_load(&ctx->venc))
        return EXIT_FAILURE;
    if (i6_isp_load(&ctx->isp))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void star6_sensor_disable(star6_context *ctx) {
    if (ctx->sensor_index >= 0)
        ctx->snr.fnDisable(ctx->sensor_index);
}

void star6_context_deinit(star6_context *ctx) {
    if (!ctx)
        return;

    star6_pipeline_stop(ctx);

    star6_sensor_disable(ctx);

    if (ctx->sys.handle)
        ctx->sys.fnExit();

    i6_isp_unload(&ctx->isp);
    i6_venc_unload(&ctx->venc);
    i6_vpe_unload(&ctx->vpe);
    i6_vif_unload(&ctx->vif);
    i6_snr_unload(&ctx->snr);
    i6_sys_unload(&ctx->sys);
}

static int star6_find_sensor(star6_context *ctx, const char *sensor_name,
    unsigned int fps, int *res_index, i6_snr_res *selected_res) {
    if (!sensor_name || !res_index || !selected_res)
        return EXIT_FAILURE;

    for (int index = 0; index < 4; ++index) {
        if (ctx->snr.fnSetPlaneMode(index, 0))
            continue;

        unsigned int count = 0;
        if (ctx->snr.fnGetResolutionCount(index, &count))
            continue;

        i6_snr_plane plane;
        if (ctx->snr.fnGetPlaneInfo(index, 0, &plane))
            continue;

        if (strcasecmp(plane.sensName, sensor_name) != 0)
            continue;

        int chosen = -1;
        unsigned long chosen_area = 0;
        for (unsigned int i = 0; i < count; ++i) {
            i6_snr_res res;
            if (ctx->snr.fnGetResolution(index, (unsigned char)i, &res))
                continue;
            if (fps > res.maxFps || fps < res.minFps)
                continue;
            unsigned long area = (unsigned long)res.output.width * res.output.height;
            if (area > chosen_area) {
                *selected_res = res;
                chosen = (int)i;
                chosen_area = area;
            }
        }

        if (chosen < 0)
            return EXIT_FAILURE;

        *res_index = chosen;
        ctx->sensor_index = index;
        return EXIT_SUCCESS;
    }

    return EXIT_FAILURE;
}

static int star6_setup_vif(star6_context *ctx) {
    i6_vif_dev device;
    memset(&device, 0, sizeof(device));
    device.intf = ctx->pad.intf;
    device.work = (device.intf == I6_INTF_BT656) ? I6_VIF_WORK_1MULTIPLEX : I6_VIF_WORK_RGB_REALTIME;
    device.hdr = I6_HDR_OFF;

    if (device.intf == I6_INTF_MIPI) {
        device.edge = I6_EDGE_DOUBLE;
        device.input = ctx->pad.intfAttr.mipi.input;
    } else if (device.intf == I6_INTF_BT656) {
        device.edge = ctx->pad.intfAttr.bt656.edge;
        device.sync = ctx->pad.intfAttr.bt656.sync;
        device.bitswap = ctx->pad.intfAttr.bt656.bitswap;
    }

    if (ctx->vif.fnSetDeviceConfig(ctx->vif_dev, &device))
        return EXIT_FAILURE;
    if (ctx->vif.fnEnableDevice(ctx->vif_dev))
        return EXIT_FAILURE;

    i6_vif_port port;
    memset(&port, 0, sizeof(port));
    port.capt = ctx->plane.capt;
    port.dest.width = ctx->plane.capt.width;
    port.dest.height = ctx->plane.capt.height;
    port.field = 0;
    port.interlaceOn = 0;
    port.pixFmt = plane_pixel_format(&ctx->plane);
    port.frate = I6_VIF_FRATE_FULL;
    port.frameLineCnt = 0;

    if (ctx->vif.fnSetPortConfig(ctx->vif_chn, ctx->vif_port, &port))
        return EXIT_FAILURE;
    if (ctx->vif.fnEnablePort(ctx->vif_chn, ctx->vif_port))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static int star6_setup_vpe(star6_context *ctx, bool mirror, bool flip) {
    i6_vpe_chn channel;
    memset(&channel, 0, sizeof(channel));
    channel.capt.width = ctx->plane.capt.width;
    channel.capt.height = ctx->plane.capt.height;
    channel.pixFmt = plane_pixel_format(&ctx->plane);
    channel.hdr = I6_HDR_OFF;
    channel.sensor = (i6_vpe_sens)(ctx->sensor_index + 1);
    channel.mode = I6_VPE_MODE_REALTIME;

    if (ctx->vpe.fnCreateChannel(ctx->vpe_chn, &channel))
        return EXIT_FAILURE;

    i6_vpe_para param;
    memset(&param, 0, sizeof(param));
    param.hdr = I6_HDR_OFF;
    param.level3DNR = 1;
    param.mirror = mirror;
    param.flip = flip;
    param.lensAdjOn = 0;

    if (ctx->vpe.fnSetChannelParam(ctx->vpe_chn, &param))
        return EXIT_FAILURE;
    if (ctx->vpe.fnStartChannel(ctx->vpe_chn))
        return EXIT_FAILURE;

    i6_vpe_port portCfg;
    memset(&portCfg, 0, sizeof(portCfg));
    portCfg.output.width = ctx->target_width;
    portCfg.output.height = ctx->target_height;
    portCfg.mirror = 0;
    portCfg.flip = 0;
    portCfg.pixFmt = I6_PIXFMT_YUV420SP;
    portCfg.compress = I6_COMPR_NONE;

    if (ctx->vpe.fnSetPortConfig(ctx->vpe_chn, ctx->vpe_port, &portCfg))
        return EXIT_FAILURE;
    if (ctx->vpe.fnEnablePort(ctx->vpe_chn, ctx->vpe_port))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static int star6_bind_vif_to_vpe(star6_context *ctx) {
    i6_sys_bind source = { .module = I6_SYS_MOD_VIF, .device = ctx->vif_dev,
        .channel = ctx->vif_chn, .port = ctx->vif_port };
    i6_sys_bind dest = { .module = I6_SYS_MOD_VPE, .device = ctx->vpe_dev,
        .channel = ctx->vpe_chn, .port = ctx->vpe_port };
    return ctx->sys.fnBindExt(&source, &dest, ctx->framerate, ctx->framerate,
        I6_SYS_LINK_REALTIME, 0);
}

static int star6_bind_vpe_to_venc(star6_context *ctx) {
    unsigned int device = 0;
    if (ctx->venc.fnGetChannelDeviceId(ctx->venc_chn, &device))
        return EXIT_FAILURE;

    i6_sys_bind source = { .module = I6_SYS_MOD_VPE, .device = ctx->vpe_dev,
        .channel = ctx->vpe_chn, .port = ctx->vpe_port };
    i6_sys_bind dest = { .module = I6_SYS_MOD_VENC, .device = device,
        .channel = ctx->venc_chn, .port = 0 };

    if (ctx->sys.fnBindExt(&source, &dest, ctx->framerate, ctx->framerate,
            I6_SYS_LINK_FRAMEBASE, 0))
        return EXIT_FAILURE;

    ctx->sys.fnSetOutputDepth(&dest, 2, 4);
    return EXIT_SUCCESS;
}

int star6_pipeline_start(star6_context *ctx, const char *sensor_name,
    unsigned int fps, const char *iq_path, hal_vidcodec codec,
    unsigned int bitrate_kbps, unsigned int gop, bool mirror, bool flip,
    unsigned int requested_width, unsigned int requested_height) {
    if (!ctx)
        return EXIT_FAILURE;

    if (!(fps == 30 || fps == 60 || fps == 90 || fps == 120))
        return EXIT_FAILURE;

    int res_index = -1;
    i6_snr_res chosen_res;
    memset(&chosen_res, 0, sizeof(chosen_res));
    if (star6_find_sensor(ctx, sensor_name, fps, &res_index, &chosen_res))
        return EXIT_FAILURE;

    ctx->framerate = fps;

    if (requested_width && requested_width <= chosen_res.output.width)
        ctx->target_width = requested_width;
    else
        ctx->target_width = chosen_res.output.width;

    if (requested_height && requested_height <= chosen_res.output.height)
        ctx->target_height = requested_height;
    else
        ctx->target_height = chosen_res.output.height;

    if (ctx->snr.fnSetResolution(ctx->sensor_index, (unsigned char)res_index))
        return EXIT_FAILURE;
    if (ctx->snr.fnSetFramerate(ctx->sensor_index, fps))
        return EXIT_FAILURE;
    if (ctx->snr.fnSetOrientation(ctx->sensor_index, mirror, flip))
        return EXIT_FAILURE;
    if (ctx->snr.fnGetPadInfo(ctx->sensor_index, &ctx->pad))
        return EXIT_FAILURE;
    if (ctx->snr.fnGetPlaneInfo(ctx->sensor_index, 0, &ctx->plane))
        return EXIT_FAILURE;
    if (ctx->snr.fnEnable(ctx->sensor_index))
        return EXIT_FAILURE;

    if (iq_path && *iq_path && access(iq_path, R_OK) == 0)
        ctx->isp.fnLoadChannelConfig(0, (char *)iq_path, 1234);

    if (star6_setup_vif(ctx))
        return EXIT_FAILURE;

    if (star6_setup_vpe(ctx, mirror, flip))
        return EXIT_FAILURE;

    if (star6_bind_vif_to_vpe(ctx))
        return EXIT_FAILURE;

    i6_venc_chn channel;
    memset(&channel, 0, sizeof(channel));

    switch (codec) {
        case HAL_VIDCODEC_H265:
            channel.attrib.codec = I6_VENC_CODEC_H265;
            channel.attrib.h265.maxWidth = ctx->target_width;
            channel.attrib.h265.maxHeight = ctx->target_height;
            channel.attrib.h265.bufSize = ctx->target_width * ctx->target_height;
            channel.attrib.h265.profile = 1;
            channel.attrib.h265.byFrame = 1;
            channel.attrib.h265.width = ctx->target_width;
            channel.attrib.h265.height = ctx->target_height;
            channel.attrib.h265.bFrameNum = 0;
            channel.attrib.h265.refNum = 1;
            channel.rate.mode = I6_VENC_RATEMODE_H265CBR;
            channel.rate.h265Cbr.gop = gop;
            channel.rate.h265Cbr.statTime = 1;
            channel.rate.h265Cbr.fpsNum = fps;
            channel.rate.h265Cbr.fpsDen = 1;
            channel.rate.h265Cbr.bitrate = bitrate_kbps << 10;
            break;
        case HAL_VIDCODEC_H264:
        default:
            channel.attrib.codec = I6_VENC_CODEC_H264;
            channel.attrib.h264.maxWidth = ctx->target_width;
            channel.attrib.h264.maxHeight = ctx->target_height;
            channel.attrib.h264.bufSize = ctx->target_width * ctx->target_height;
            channel.attrib.h264.profile = 2;
            channel.attrib.h264.byFrame = 1;
            channel.attrib.h264.width = ctx->target_width;
            channel.attrib.h264.height = ctx->target_height;
            channel.attrib.h264.bFrameNum = 0;
            channel.attrib.h264.refNum = 1;
            channel.rate.mode = I6_VENC_RATEMODE_H264CBR;
            channel.rate.h264Cbr.gop = gop;
            channel.rate.h264Cbr.statTime = 1;
            channel.rate.h264Cbr.fpsNum = fps;
            channel.rate.h264Cbr.fpsDen = 1;
            channel.rate.h264Cbr.bitrate = bitrate_kbps << 10;
            break;
    }

    if (ctx->venc.fnCreateChannel(ctx->venc_chn, &channel))
        return EXIT_FAILURE;

    if (ctx->venc.fnStartReceiving(ctx->venc_chn))
        return EXIT_FAILURE;

    ctx->encoder_started = true;

    if (star6_bind_vpe_to_venc(ctx))
        return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

static void star6_unbind_vpe_to_venc(star6_context *ctx) {
    unsigned int device = 0;
    if (ctx->venc.fnGetChannelDeviceId(ctx->venc_chn, &device))
        return;

    i6_sys_bind source = { .module = I6_SYS_MOD_VPE, .device = ctx->vpe_dev,
        .channel = ctx->vpe_chn, .port = ctx->vpe_port };
    i6_sys_bind dest = { .module = I6_SYS_MOD_VENC, .device = device,
        .channel = ctx->venc_chn, .port = 0 };

    ctx->sys.fnUnbind(&source, &dest);
}

static void star6_unbind_vif_to_vpe(star6_context *ctx) {
    i6_sys_bind source = { .module = I6_SYS_MOD_VIF, .device = ctx->vif_dev,
        .channel = ctx->vif_chn, .port = ctx->vif_port };
    i6_sys_bind dest = { .module = I6_SYS_MOD_VPE, .device = ctx->vpe_dev,
        .channel = ctx->vpe_chn, .port = ctx->vpe_port };

    ctx->sys.fnUnbind(&source, &dest);
}

void star6_pipeline_stop(star6_context *ctx) {
    if (!ctx)
        return;

    if (ctx->encoder_started) {
        ctx->venc.fnStopReceiving(ctx->venc_chn);
        star6_unbind_vpe_to_venc(ctx);
        ctx->venc.fnDestroyChannel(ctx->venc_chn);
        ctx->encoder_started = false;
    }

    ctx->vpe.fnDisablePort(ctx->vpe_chn, ctx->vpe_port);
    ctx->vpe.fnStopChannel(ctx->vpe_chn);
    ctx->vpe.fnDestroyChannel(ctx->vpe_chn);

    star6_unbind_vif_to_vpe(ctx);

    ctx->vif.fnDisablePort(ctx->vif_chn, ctx->vif_port);
    ctx->vif.fnDisableDevice(ctx->vif_dev);

    star6_sensor_disable(ctx);
}

int star6_query_stream(star6_context *ctx, i6_venc_stat *stat) {
    if (!ctx || !stat)
        return EXIT_FAILURE;
    return ctx->venc.fnQuery(ctx->venc_chn, stat);
}

int star6_acquire_stream(star6_context *ctx, i6_venc_strm *stream, unsigned int timeout_ms) {
    if (!ctx || !stream)
        return EXIT_FAILURE;
    return ctx->venc.fnGetStream(ctx->venc_chn, stream, timeout_ms);
}

int star6_release_stream(star6_context *ctx, i6_venc_strm *stream) {
    if (!ctx || !stream)
        return EXIT_FAILURE;
    return ctx->venc.fnFreeStream(ctx->venc_chn, stream);
}

