#
# Copyright 2025 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#

OS = $(shell uname)
PLATFORM = $(shell uname -a)

BASE_DIR := $(shell pwd)

ONEWIFI_HOME = $(BASE_DIR)/../../../OneWifi
WLAN_EMU_HOME = $(BASE_DIR)/../../../OneWifiTestSuite
WIFI_HAL_INTERFACE = $(BASE_DIR)/../../../halinterface/include
WLAN_EMU_SRC = $(WLAN_EMU_HOME)/src
WLAN_EMU_KERNEL_SRC = $(WLAN_EMU_HOME)/samples/linux-master
WLAN_EMU_KERNEL_ARCH_SRC = $(WLAN_EMU_KERNEL_SRC)/arch/x86
WLAN_EMU_KERNEL_ARCH_ASM_INC = $(WLAN_EMU_KERNEL_SRC)/arch/arm64/include
WIFI_RDK_HAL = $(BASE_DIR)/../../../rdk-wifi-hal
WIFI_HOSTAP_BASE = $(BASE_DIR)/../../../rdk-wifi-libhostap/source/hostap-2.11
WIFI_HOSTAP_SRC = $(WIFI_HOSTAP_BASE)/src
WIFI_OPENSSL_DIR = /home/yuriy.masechko/dev/openssl/openssl/
WIFI_HOSTAP_SUPPLICANT = $(WIFI_HOSTAP_BASE)/wpa_supplicant

INSTALLDIR = $(WLAN_EMU_HOME)/install
OBJDIR = obj
CXX = $(TARGET_CXX)
CC = $(TARGET_CC)
AR ?= $(TARGET_AR)
RM = -rm -rf
LDFLAGS = $(TARGET_LDFLAGS)
