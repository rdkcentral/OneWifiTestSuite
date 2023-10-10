OS = $(shell uname)
PLATFORM = $(shell uname -a)

WLAN_EMU_HOME = /media/sf_work/wifi_programs/wlan_emulator
ONEWIFI_HOME = /media/sf_work/wifi_programs/one_wifi_program
WLAN_EMU_SRC = $(WLAN_EMU_HOME)/src
WLAN_EMU_KERNEL_SRC = $(WLAN_EMU_HOME)/samples/linux-master
WLAN_EMU_KERNEL_ARCH_SRC = $(WLAN_EMU_KERNEL_SRC)/arch/x86
WLAN_EMU_KERNEL_ARCH_ASM_INC = $(WLAN_EMU_KERNEL_SRC)/arch/arm64/include
WIFI_RDK_HAL = $(ONEWIFI_HOME)/rdk-wifi-hal
WIFI_CJSON = $(ONEWIFI_HOME)/cJSON-master
WIFI_RDK_HAL = $(ONEWIFI_HOME)/rdk-wifi-hal
WIFI_HOSTAP_BASE = $(ONEWIFI_HOME)/rdk-wifi-libhostap/source/hostap-2.9
WIFI_HOSTAP_SRC = $(WIFI_HOSTAP_BASE)/src
WIFI_LIB_PCAP = $(ONEWIFI_HOME)/libpcap-master-2
WIFI_OPENSSL_DIR = /home/soumya/Work/wifi_programs/openssl-master

INSTALLDIR = $(WLAN_EMU_HOME)/install
OBJDIR = obj
CXX = g++
CC = gcc
AR= ar cr
RM = -rm -rf
