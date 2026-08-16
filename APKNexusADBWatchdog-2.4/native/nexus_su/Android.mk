LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := nexus_su
LOCAL_SRC_FILES := nexus_su.c
LOCAL_CFLAGS := -Wall -Wextra -O2 -fPIE
LOCAL_LDFLAGS := -fPIE -pie
include $(BUILD_EXECUTABLE)
