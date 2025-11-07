#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/types.h"
#include "hal/star/i6_common.h"
#include "hal/star/i6_sys.h"
#include "hal/star/i6_snr.h"
#include "hal/star/i6_vif.h"
#include "hal/star/i6_vpe.h"
#include "hal/star/i6_venc.h"
#include "hal/star/i6_isp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i6_sys_impl sys;
    i6_snr_impl snr;
    i6_vif_impl vif;
    i6_vpe_impl vpe;
    i6_venc_impl venc;
    i6_isp_impl isp;

    int sensor_index;
    i6_snr_pad pad;
    i6_snr_plane plane;

    unsigned int target_width;
    unsigned int target_height;
    unsigned int framerate;

    int vif_dev;
    int vif_chn;
    int vif_port;

    int vpe_dev;
    int vpe_chn;
    int vpe_port;

    int venc_chn;
    bool encoder_started;
} star6_context;

int star6_context_init(star6_context *ctx);
void star6_context_deinit(star6_context *ctx);

int star6_pipeline_start(star6_context *ctx, const char *sensor_name,
    unsigned int fps, const char *iq_path, hal_vidcodec codec,
    unsigned int bitrate_kbps, unsigned int gop, bool mirror, bool flip,
    unsigned int requested_width, unsigned int requested_height);

void star6_pipeline_stop(star6_context *ctx);

int star6_query_stream(star6_context *ctx, i6_venc_stat *stat);
int star6_acquire_stream(star6_context *ctx, i6_venc_strm *stream, unsigned int timeout_ms);
int star6_release_stream(star6_context *ctx, i6_venc_strm *stream);

#ifdef __cplusplus
}
#endif

