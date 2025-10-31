#include "main.h"
#include "star6e.h"
#include <signal.h>
#include <stdbool.h>
#include <time.h>

static volatile bool g_running = true;

static void handle_signal(int sig) {
  (void)sig;
  g_running = false;
}

static MI_VENC_RcMode_e translate_rc_mode(PAYLOAD_TYPE_E codec, int rc_mode) {
  if (codec == PT_H265) {
    switch (rc_mode) {
      case 3: return E_MI_VENC_RC_MODE_H265CBR;
      case 4: return E_MI_VENC_RC_MODE_H265VBR;
      case 5: return E_MI_VENC_RC_MODE_H265AVBR;
      case 6: return E_MI_VENC_RC_MODE_H265QVBR;
      default: return E_MI_VENC_RC_MODE_H265CBR;
    }
  }

  switch (rc_mode) {
    case 0: return E_MI_VENC_RC_MODE_H264AVBR;
    case 1: return E_MI_VENC_RC_MODE_H264QVBR;
    case 2: return E_MI_VENC_RC_MODE_H264VBR;
    case 3: return E_MI_VENC_RC_MODE_H264CBR;
    default: return E_MI_VENC_RC_MODE_H264AVBR;
  }
}

static MI_VENC_ModType_e translate_codec(PAYLOAD_TYPE_E codec) {
  switch (codec) {
    case PT_H265:
      return E_MI_VENC_MODTYPE_H265;
    case PT_H264:
    default:
      return E_MI_VENC_MODTYPE_H264;
  }
}

static int start_sensor(uint32_t width, uint32_t height) {
  MI_S32 ret = MI_SNR_SetPlaneMode(E_MI_SNR_PAD_ID_0, E_MI_SNR_PLANE_MODE_LINEAR);
  if (ret != 0) {
    printf("ERROR: MI_SNR_SetPlaneMode failed %d\n", ret);
    return ret;
  }

  ret = MI_SNR_SetRes(E_MI_SNR_PAD_ID_0, 0);
  if (ret != 0) {
    printf("ERROR: MI_SNR_SetRes failed %d\n", ret);
    return ret;
  }

  ret = MI_SNR_Enable(E_MI_SNR_PAD_ID_0);
  if (ret != 0) {
    printf("ERROR: MI_SNR_Enable failed %d\n", ret);
    return ret;
  }

  (void)width;
  (void)height;
  return 0;
}

static int stop_sensor(void) {
  return MI_SNR_Disable(E_MI_SNR_PAD_ID_0);
}

static int start_vif(uint32_t width, uint32_t height) {
  MI_VIF_DevAttr_t dev_attr = {
    .eIntfMode = E_MI_VIF_MODE_MIPI,
    .eWorkMode = E_MI_VIF_WORK_MODE_1MULTIPLEX,
    .eHDRType = E_MI_VIF_HDR_TYPE_OFF,
  };

  MI_S32 ret = MI_VIF_CreateDev(0, &dev_attr);
  if (ret != 0) {
    printf("ERROR: MI_VIF_CreateDev failed %d\n", ret);
    return ret;
  }

  ret = MI_VIF_EnableDev(0);
  if (ret != 0) {
    printf("ERROR: MI_VIF_EnableDev failed %d\n", ret);
    return ret;
  }

  MI_VIF_PortAttr_t port_attr = {
    .u16Width = width,
    .u16Height = height,
    .ePixFormat = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420,
  };

  ret = MI_VIF_CreatePort(0, &port_attr);
  if (ret != 0) {
    printf("ERROR: MI_VIF_CreatePort failed %d\n", ret);
    return ret;
  }

  ret = MI_VIF_EnablePort(0);
  if (ret != 0) {
    printf("ERROR: MI_VIF_EnablePort failed %d\n", ret);
    return ret;
  }

  return 0;
}

static void stop_vif(void) {
  MI_VIF_DisablePort(0);
  MI_VIF_DestroyPort(0);
  MI_VIF_DisableDev(0);
  MI_VIF_DestroyDev(0);
}

static int start_vpe(uint32_t width, uint32_t height) {
  MI_VPE_ChannelAttr_t ch_attr = {
    .u16MaxW = width,
    .u16MaxH = height,
    .eFormat = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420,
  };

  MI_S32 ret = MI_VPE_CreateChannel(0, &ch_attr);
  if (ret != 0) {
    printf("ERROR: MI_VPE_CreateChannel failed %d\n", ret);
    return ret;
  }

  ret = MI_VPE_StartChannel(0);
  if (ret != 0) {
    printf("ERROR: MI_VPE_StartChannel failed %d\n", ret);
    return ret;
  }

  MI_VPE_PortAttr_t port_attr = {
    .u16Width = width,
    .u16Height = height,
    .eFormat = E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420,
  };

  ret = MI_VPE_SetPortAttr(0, 0, &port_attr);
  if (ret != 0) {
    printf("ERROR: MI_VPE_SetPortAttr failed %d\n", ret);
    return ret;
  }

  ret = MI_VPE_EnablePort(0, 0);
  if (ret != 0) {
    printf("ERROR: MI_VPE_EnablePort failed %d\n", ret);
    return ret;
  }

  return 0;
}

static void stop_vpe(void) {
  MI_VPE_DisablePort(0, 0);
  MI_VPE_StopChannel(0);
  MI_VPE_DestroyChannel(0);
}

static int start_venc(uint32_t width, uint32_t height, uint32_t bitrate,
  uint32_t framerate, uint32_t gop, PAYLOAD_TYPE_E codec,
  int rc_mode, MI_VENC_CHN* chn)
{
  MI_VENC_ChnAttr_t attr = {
    .eType = translate_codec(codec),
    .u32PicWidth = width,
    .u32PicHeight = height,
    .u32MaxBitRate = bitrate * 1024,
    .u32SrcFrmRateNum = framerate,
    .u32Gop = gop,
    .eRcMode = translate_rc_mode(codec, rc_mode),
  };

  *chn = 0;
  MI_S32 ret = MI_VENC_CreateChn(*chn, &attr);
  if (ret != 0) {
    printf("ERROR: MI_VENC_CreateChn failed %d\n", ret);
    return ret;
  }

  ret = MI_VENC_StartRecvPic(*chn);
  if (ret != 0) {
    printf("ERROR: MI_VENC_StartRecvPic failed %d\n", ret);
    return ret;
  }

  return 0;
}

static void stop_venc(MI_VENC_CHN chn) {
  MI_VENC_StopRecvPic(chn);
  MI_VENC_DestroyChn(chn);
}

int main(int argc, const char* argv[]) {
  if (argc == 2 && !strcmp(argv[1], "help")) {
    printHelp();
    return 1;
  }

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  uint32_t sensor_width = 1920;
  uint32_t sensor_height = 1080;
  uint32_t image_width = sensor_width;
  uint32_t image_height = sensor_height;
  uint32_t sensor_framerate = 30;
  uint32_t venc_max_rate = 8192;
  uint32_t venc_gop_denom = 10;
  PAYLOAD_TYPE_E rc_codec = PT_H264;
  int rc_mode = 0;
  uint32_t venc_slice_size = 4;
  uint32_t venc_gop_size = sensor_framerate / venc_gop_denom;
  uint32_t udp_sink_ip = inet_addr("127.0.0.1");
  uint16_t udp_sink_port = 5000;
  uint16_t max_frame_size = 1400;
  int enable_slices = 1;
  int enable_roi = 0;
  int enable_lowdelay = 0;
  uint16_t roi_qp = 20;
  HI_BOOL venc_by_frame = HI_FALSE;
  bool limit_exposure = false;
  int image_mirror = 0;
  int image_flip = 0;

  __BeginParseConsoleArguments__(printHelp)
    __OnArgument("-h") {
      udp_sink_ip = inet_addr(__ArgValue);
      continue;
    }

    __OnArgument("-p") {
      udp_sink_port = atoi(__ArgValue);
      continue;
    }

    __OnArgument("-r") {
      venc_max_rate = atoi(__ArgValue);
      continue;
    }

    __OnArgument("-n") {
      max_frame_size = atoi(__ArgValue);
      continue;
    }

    __OnArgument("-m") {
      const char* value = __ArgValue;
      if (strcmp(value, "compact") && strcmp(value, "rtp")) {
        printf("> ERROR: Unknown streaming mode\n");
        return 1;
      }
      continue;
    }

    __OnArgument("-v") {
      const char* value = __ArgValue;
      if (!strcmp(value, "star6e_imx335")) {
        sensor_width = image_width = 2592;
        sensor_height = image_height = 1944;
        sensor_framerate = 30;
      } else if (!strcmp(value, "star6e_os04a")) {
        sensor_width = image_width = 2688;
        sensor_height = image_height = 1520;
        sensor_framerate = 30;
      } else if (!strcmp(value, "720p")) {
        sensor_width = image_width = 1280;
        sensor_height = image_height = 720;
      } else if (!strcmp(value, "1080p")) {
        sensor_width = image_width = 1920;
        sensor_height = image_height = 1080;
      } else {
        printf("> ERROR: Unsupported version %s\n", value);
        return 1;
      }
      continue;
    }

    __OnArgument("-f") {
      sensor_framerate = atoi(__ArgValue);
      continue;
    }

    __OnArgument("-g") {
      venc_gop_denom = atoi(__ArgValue);
      continue;
    }

    __OnArgument("-c") {
      const char* value = __ArgValue;
      if (!strcmp(value, "264avbr")) {
        rc_codec = PT_H264;
        rc_mode = 0;
      } else if (!strcmp(value, "264qvbr")) {
        rc_codec = PT_H264;
        rc_mode = 1;
      } else if (!strcmp(value, "264vbr")) {
        rc_codec = PT_H264;
        rc_mode = 2;
      } else if (!strcmp(value, "264cbr")) {
        rc_codec = PT_H264;
        rc_mode = 3;
      } else if (!strcmp(value, "265avbr")) {
        rc_codec = PT_H265;
        rc_mode = 5;
      } else if (!strcmp(value, "265qvbr")) {
        rc_codec = PT_H265;
        rc_mode = 6;
      } else if (!strcmp(value, "265vbr")) {
        rc_codec = PT_H265;
        rc_mode = 4;
      } else if (!strcmp(value, "265cbr")) {
        rc_codec = PT_H265;
        rc_mode = 3;
      } else {
        printf("> ERROR: Unsupported codec %s\n", value);
        return 1;
      }
      continue;
    }

    __OnArgument("-s") {
      const char* value = __ArgValue;
      if (!strcmp(value, "720p")) {
        image_width = sensor_width = 1280;
        image_height = sensor_height = 720;
      } else if (!strcmp(value, "1080p")) {
        image_width = sensor_width = 1920;
        image_height = sensor_height = 1080;
      } else if (!strcmp(value, "4MP")) {
        image_width = sensor_width = 2688;
        image_height = sensor_height = 1520;
      } else if (sscanf(value, "%ux%u", &image_width, &image_height) == 2) {
        sensor_width = image_width;
        sensor_height = image_height;
      } else {
        printf("> ERROR: Unsupported resolution %s\n", value);
        return 1;
      }
      continue;
    }

    __OnArgument("--no-slices") {
      enable_slices = 0;
      continue;
    }

    __OnArgument("--slice-size") {
      venc_slice_size = atoi(__ArgValue);
      continue;
    }

    __OnArgument("--low-delay") {
      enable_lowdelay = 1;
      continue;
    }

    __OnArgument("--roi") {
      enable_roi = 1;
      continue;
    }

    __OnArgument("--roi-qp") {
      roi_qp = atoi(__ArgValue);
      continue;
    }

    __OnArgument("--mirror") {
      image_mirror = 1;
      continue;
    }

    __OnArgument("--flip") {
      image_flip = 1;
      continue;
    }

    __OnArgument("--exp") {
      limit_exposure = true;
      continue;
    }
  __EndParseConsoleArguments__

  venc_gop_size = sensor_framerate / (venc_gop_denom ? venc_gop_denom : 1);

  printf("> Starting star6e pipeline\n");
  printf("  - Sensor: %ux%u @ %u\n", sensor_width, sensor_height, sensor_framerate);
  printf("  - Image : %ux%u\n", image_width, image_height);

  (void)enable_slices;
  (void)enable_lowdelay;
  (void)enable_roi;
  (void)roi_qp;
  (void)venc_slice_size;
  (void)venc_by_frame;
  (void)limit_exposure;
  (void)image_mirror;
  (void)image_flip;

  int ret = MI_SYS_Init();
  if (ret != 0) {
    printf("ERROR: MI_SYS_Init failed %d\n", ret);
    return ret;
  }

  ret = start_sensor(sensor_width, sensor_height);
  if (ret != 0) {
    goto cleanup_sys;
  }

  ret = start_vif(sensor_width, sensor_height);
  if (ret != 0) {
    goto cleanup_sensor;
  }

  ret = start_vpe(image_width, image_height);
  if (ret != 0) {
    goto cleanup_vif;
  }

  MI_VENC_CHN venc_channel = 0;
  ret = start_venc(image_width, image_height, venc_max_rate,
    sensor_framerate, venc_gop_size, rc_codec, rc_mode, &venc_channel);
  if (ret != 0) {
    goto cleanup_vpe;
  }

  MI_SYS_ChnPort_t vif_port = {
    .eModId = E_MI_MODULE_ID_VIF,
    .s32DevId = 0,
    .s32ChnId = 0,
    .s32PortId = 0,
  };
  MI_SYS_ChnPort_t vpe_port = {
    .eModId = E_MI_MODULE_ID_VPE,
    .s32DevId = 0,
    .s32ChnId = 0,
    .s32PortId = 0,
  };
  MI_SYS_ChnPort_t venc_port = {
    .eModId = E_MI_MODULE_ID_VENC,
    .s32DevId = 0,
    .s32ChnId = venc_channel,
    .s32PortId = 0,
  };

  int bound_vif_vpe = 0;
  int bound_vpe_venc = 0;

  ret = MI_SYS_Bind(&vif_port, &vpe_port);
  if (ret != 0) {
    printf("ERROR: MI_SYS_Bind VIF->VPE failed %d\n", ret);
    goto cleanup_venc;
  }
  bound_vif_vpe = 1;

  ret = MI_SYS_Bind(&vpe_port, &venc_port);
  if (ret != 0) {
    printf("ERROR: MI_SYS_Bind VPE->VENC failed %d\n", ret);
    goto cleanup_venc;
  }
  bound_vpe_venc = 1;
  MI_SYS_SetChnOutputPortDepth(&venc_port, 2, 6);

  int socket_handle = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_handle < 0) {
    printf("ERROR: Unable to create UDP socket\n");
    goto cleanup_venc;
  }

  struct sockaddr_in dst;
  memset(&dst, 0, sizeof(dst));
  dst.sin_family = AF_INET;
  dst.sin_port = htons(udp_sink_port);
  dst.sin_addr.s_addr = udp_sink_ip;

  while (g_running) {
    MI_VENC_Stream_t stream;
    ret = MI_VENC_GetStream(venc_channel, &stream, 1000);
    if (ret != 0) {
      continue;
    }

    sendPacket(stream.pStream, stream.u32Len, socket_handle,
      (struct sockaddr*)&dst, max_frame_size);

    MI_VENC_ReleaseStream(venc_channel, &stream);
  }

  close(socket_handle);

cleanup_venc:
  if (bound_vpe_venc) {
    MI_SYS_UnBind(&vpe_port, &venc_port);
  }
  if (bound_vif_vpe) {
    MI_SYS_UnBind(&vif_port, &vpe_port);
  }
  stop_venc(venc_channel);

cleanup_vpe:
  stop_vpe();

cleanup_vif:
  stop_vif();

cleanup_sensor:
  stop_sensor();

cleanup_sys:
  MI_SYS_Exit();

  return ret;
}
