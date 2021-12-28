LOCAL_PATH:= $(call my-dir)

$(call add-prebuilt-files, STATIC_LIBRARIES, libvcp.a)

# audio preprocessing wrapper
include $(CLEAR_VARS)

LOCAL_MODULE:= libaudiopreprocessing
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/soundfx

LOCAL_SRC_FILES:= \
    PreProcessing.cpp validate.c  vcp_ci.c  vcp_internal.c wav_file.c

LOCAL_C_INCLUDES +=  system/media/audio_effects/include

LOCAL_STATIC_LIBRARIES := libvcp 
LOCAL_SHARED_LIBRARIES := \
    libutils \
    liblog

CONFIG_WITHOUT_BRT_BT_MODULE := n
ifeq ($(CONFIG_WITHOUT_BRT_BT_MODULE),y)
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../ProductActivation/BsPA_Sdk/ \
	$(LOCAL_PATH)/../ProductActivation/BsPA_CommFunc/ \
	$(LOCAL_PATH)/../ProductActivation/BsPA_packet/ \
	$(LOCAL_PATH)/../ProductActivation/BsPA_Rsa/ \
	$(LOCAL_PATH)/../ProductActivation/BsPA_MD5/ \
	$(LOCAL_PATH)/../ProductActivation/BsPA_HWValue/ \
	$(LOCAL_PATH)/../ProductActivation/curl/ 
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../chklic

LOCAL_STATIC_LIBRARIES += libcurl libBsPA_Sdk libBsPA_Packet libBsPA_CommFunc libBsPA_Rsa libBsPA_MD5 libBsPA_HWValue libchklic
LOCAL_CFLAGS += -DCONFIG_WITHOUT_BRT_BT_MODULE
endif

ifeq ($(TARGET_SIMULATOR),true)
LOCAL_LDLIBS += -ldl
else
LOCAL_SHARED_LIBRARIES += libdl
endif

LOCAL_CFLAGS += -fvisibility=hidden

include $(BUILD_SHARED_LIBRARY)
