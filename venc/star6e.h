#ifndef STAR6E_PLATFORM_H
#define STAR6E_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal SigmaStar SDK type definitions */

typedef int32_t MI_S32;
typedef uint32_t MI_U32;
typedef uint16_t MI_U16;
typedef uint8_t MI_U8;
typedef uint64_t MI_U64;
typedef bool MI_BOOL;

typedef enum {
  E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_420,
  E_MI_SYS_PIXEL_FRAME_YUV_SEMIPLANAR_422,
} MI_SYS_PixelFormat_e;

typedef enum {
  E_MI_SYS_FRAME_SCAN_MODE_PROGRESSIVE,
} MI_SYS_FrameScanMode_e;

typedef struct {
  MI_U16 u16Width;
  MI_U16 u16Height;
} MI_SYS_WindowRect_t;

typedef struct {
  MI_SYS_PixelFormat_e ePixelFormat;
  MI_SYS_FrameScanMode_e eFrameScanMode;
  MI_U16 u16Width;
  MI_U16 u16Height;
  MI_U16 u16CropWidth;
  MI_U16 u16CropHeight;
  MI_U32 u32ChnBufNum;
} MI_SYS_ChnPortAttr_t;

typedef enum {
  E_MI_MODULE_ID_VIF,
  E_MI_MODULE_ID_VPE,
  E_MI_MODULE_ID_VENC,
} MI_ModuleId_e;

typedef struct {
  MI_ModuleId_e eModId;
  MI_S32 s32DevId;
  MI_S32 s32ChnId;
  MI_S32 s32PortId;
} MI_SYS_ChnPort_t;

/* sensor */
typedef enum {
  E_MI_SNR_PAD_ID_0,
} MI_SNR_PAD_ID_e;

typedef enum {
  E_MI_SNR_PLANE_MODE_LINEAR,
} MI_SNR_PlaneMode_e;

typedef struct {
  MI_U16 u16Width;
  MI_U16 u16Height;
  MI_U32 u32MinFps;
  MI_U32 u32MaxFps;
} MI_SNR_Res_t;

MI_S32 MI_SYS_Init(void);
MI_S32 MI_SYS_Exit(void);
MI_S32 MI_SYS_SetChnOutputPortDepth(const MI_SYS_ChnPort_t* chn_port, MI_U32 user_depth, MI_U32 buf_depth);
MI_S32 MI_SYS_Bind(const MI_SYS_ChnPort_t* src_chn_port, const MI_SYS_ChnPort_t* dst_chn_port);
MI_S32 MI_SYS_UnBind(const MI_SYS_ChnPort_t* src_chn_port, const MI_SYS_ChnPort_t* dst_chn_port);

MI_S32 MI_SNR_SetPlaneMode(MI_SNR_PAD_ID_e pad_id, MI_SNR_PlaneMode_e mode);
MI_S32 MI_SNR_SetRes(MI_SNR_PAD_ID_e pad_id, MI_U32 res_idx);
MI_S32 MI_SNR_Enable(MI_SNR_PAD_ID_e pad_id);
MI_S32 MI_SNR_Disable(MI_SNR_PAD_ID_e pad_id);

/* VIF */
typedef int MI_VIF_DEV;
typedef int MI_VIF_CHN;
typedef int MI_VIF_PORT;

typedef enum {
  E_MI_VIF_MODE_MIPI,
} MI_VIF_IntfMode_e;

typedef enum {
  E_MI_VIF_WORK_MODE_1MULTIPLEX,
} MI_VIF_WorkMode_e;

typedef enum {
  E_MI_VIF_HDR_TYPE_OFF,
} MI_VIF_HDRType_e;

typedef struct {
  MI_VIF_IntfMode_e eIntfMode;
  MI_VIF_WorkMode_e eWorkMode;
  MI_VIF_HDRType_e eHDRType;
} MI_VIF_DevAttr_t;

MI_S32 MI_VIF_CreateDev(MI_VIF_DEV dev, MI_VIF_DevAttr_t* attr);
MI_S32 MI_VIF_DestroyDev(MI_VIF_DEV dev);
MI_S32 MI_VIF_SetDevAttr(MI_VIF_DEV dev, MI_VIF_DevAttr_t* attr);
MI_S32 MI_VIF_EnableDev(MI_VIF_DEV dev);
MI_S32 MI_VIF_DisableDev(MI_VIF_DEV dev);

typedef struct {
  MI_U16 u16Width;
  MI_U16 u16Height;
  MI_SYS_PixelFormat_e ePixFormat;
} MI_VIF_PortAttr_t;

MI_S32 MI_VIF_CreatePort(MI_VIF_PORT port, MI_VIF_PortAttr_t* attr);
MI_S32 MI_VIF_DestroyPort(MI_VIF_PORT port);
MI_S32 MI_VIF_SetPortAttr(MI_VIF_PORT port, MI_VIF_PortAttr_t* attr);
MI_S32 MI_VIF_EnablePort(MI_VIF_PORT port);
MI_S32 MI_VIF_DisablePort(MI_VIF_PORT port);

/* VPE */
typedef int MI_VPE_CHANNEL;
typedef int MI_VPE_PORT;

typedef struct {
  MI_U16 u16MaxW;
  MI_U16 u16MaxH;
  MI_SYS_PixelFormat_e eFormat;
} MI_VPE_ChannelAttr_t;

typedef struct {
  MI_U16 u16Width;
  MI_U16 u16Height;
  MI_SYS_PixelFormat_e eFormat;
} MI_VPE_PortAttr_t;

MI_S32 MI_VPE_CreateChannel(MI_VPE_CHANNEL chn, MI_VPE_ChannelAttr_t* attr);
MI_S32 MI_VPE_DestroyChannel(MI_VPE_CHANNEL chn);
MI_S32 MI_VPE_StartChannel(MI_VPE_CHANNEL chn);
MI_S32 MI_VPE_StopChannel(MI_VPE_CHANNEL chn);
MI_S32 MI_VPE_SetPortAttr(MI_VPE_CHANNEL chn, MI_VPE_PORT port, MI_VPE_PortAttr_t* attr);
MI_S32 MI_VPE_EnablePort(MI_VPE_CHANNEL chn, MI_VPE_PORT port);
MI_S32 MI_VPE_DisablePort(MI_VPE_CHANNEL chn, MI_VPE_PORT port);

/* VENC */
typedef int MI_VENC_CHN;

typedef enum {
  E_MI_VENC_RC_MODE_H264CBR,
  E_MI_VENC_RC_MODE_H264VBR,
  E_MI_VENC_RC_MODE_H264AVBR,
  E_MI_VENC_RC_MODE_H264QVBR,
  E_MI_VENC_RC_MODE_H265CBR,
  E_MI_VENC_RC_MODE_H265VBR,
  E_MI_VENC_RC_MODE_H265AVBR,
  E_MI_VENC_RC_MODE_H265QVBR,
} MI_VENC_RcMode_e;

typedef enum {
  E_MI_VENC_MODTYPE_H264,
  E_MI_VENC_MODTYPE_H265,
} MI_VENC_ModType_e;

typedef struct {
  MI_VENC_ModType_e eType;
  MI_U32 u32PicWidth;
  MI_U32 u32PicHeight;
  MI_U32 u32MaxBitRate;
  MI_U32 u32SrcFrmRateNum;
  MI_U32 u32Gop;
  MI_VENC_RcMode_e eRcMode;
} MI_VENC_ChnAttr_t;

MI_S32 MI_VENC_CreateChn(MI_VENC_CHN chn, MI_VENC_ChnAttr_t* attr);
MI_S32 MI_VENC_DestroyChn(MI_VENC_CHN chn);
MI_S32 MI_VENC_StartRecvPic(MI_VENC_CHN chn);
MI_S32 MI_VENC_StopRecvPic(MI_VENC_CHN chn);

typedef struct {
  MI_U8* pStream;
  MI_U32 u32Len;
  MI_U64 u64Pts;
} MI_VENC_Stream_t;

MI_S32 MI_VENC_GetStream(MI_VENC_CHN chn, MI_VENC_Stream_t* stream, MI_S32 timeout_ms);
MI_S32 MI_VENC_ReleaseStream(MI_VENC_CHN chn, MI_VENC_Stream_t* stream);

#ifdef __cplusplus
}
#endif

#endif
