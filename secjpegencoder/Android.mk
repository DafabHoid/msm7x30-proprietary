# Samsung JPEG hardware encoder wrapper library
# Copyright (C) 2018 DafabHoid
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    SecJpegEncoder.cpp \

LOCAL_CFLAGS := -std=c11
LOCAL_ARM_MODE := arm
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MODULE := secjpegencoder
LOCAL_MODULE_TAGS := optional

include $(BUILD_SHARED_LIBRARY)

