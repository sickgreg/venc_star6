VENC_COMMON := shared.c
VENC_HI := main.c common.c compat.c isp_profiles.c mipi_profiles.c vi_profiles.c
VENC_STAR6E := star6e_main.c
SENSOR = $(SDK)/sensor/imx307_2l_cmos.c $(SDK)/sensor/imx307_2l_sensor_ctl.c \
        $(SDK)/sensor/imx335_cmos.c $(SDK)/sensor/imx335_sensor_ctl.c
CFLAGS ?= -Os -s
BUILD = $(CC) $(CFLAGS) $(VENC_COMMON) $(VENC) $(SENSOR) -I $(SDK)/include -L $(DRV) $(LIB) -o venc

venc-goke:
        $(eval SDK = ../sdk/gk7205v300)
        $(eval VENC = $(VENC_HI))
        $(eval LIB = -lhi_mpi -lhi_isp -lhi_ae -lhi_awb -lgk_api -lgk_isp -lgk_ae -lgk_awb \
                -ldehaze -ldrc -lldci -lir_auto -ldnvqe -lupvqe -lvoice_engine -lsecurec)
        $(BUILD)

venc-hisi:
        $(eval SDK = ../sdk/hi3516ev300)
        $(eval VENC = $(VENC_HI))
        $(eval LIB = -lisp -lmpi -ldnvqe -lupvqe -l_hiae -l_hiawb \
                -l_hildci -l_hidrc -l_hidehaze -lVoiceEngine -lsecurec)
        $(BUILD)

venc-star6e:
        $(eval SDK = ../sdk/ssc338q)
        $(eval VENC = $(VENC_STAR6E))
        $(eval SENSOR = )
        $(eval LIB = -lmi_sys -lmi_vif -lmi_vpe -lmi_venc -lmi_sensor -lmi_isp)
        $(eval CFLAGS += -DPLATFORM_STAR6E)
        $(BUILD)
