#
# Copyright 2025 Comcast Cable Communications Management, LLC

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0
#
#

include makefile.inc

#
# program
#
PROGRAM = $(WLAN_EMU_HOME)/install/bin/cci

# wifi hal rules
HAL_LIBRARY = $(INSTALLDIR)/lib/libwifihalcci.a
WEBCONFIG_LIBRARY = $(ONEWIFI_HOME)/install/lib/libwebconfig.a

INCLUDE_HAL_LIB_DIRS = \
    -I$(ONEWIFI_HOME)/include \
    -I$(WIFI_RDK_HAL)/src \
    -I$(WIFI_RDK_HAL)/util_crypto \
    -I$(WIFI_SYSCALL_WRAPPER)/source/ \
    -I$(WIFI_CJSON)/

INCLUDE_HOSTAP_LIB_DIRS = \
    -I$(WIFI_HOSTAP_BASE) \
    -I$(WIFI_HOSTAP_SUPPLICANT) \
    -I$(WIFI_HOSTAP_SRC) \
    -I$(WIFI_HOSTAP_SRC)/ap \
    -I$(WIFI_HOSTAP_SRC)/utils \
    -I$(WIFI_HOSTAP_SRC)/drivers \
    -I$(WIFI_HOSTAP_SRC)/common \

HAL_LIB_SOURCES = $(filter-out $(WIFI_RDK_HAL)/src/wifi_hal_frame_test.c $(WIFI_RDK_HAL)/src/wifi_hal_dpp.c, $(wildcard $(WIFI_RDK_HAL)/src/*.c)) \
    $(wildcard $(WIFI_RDK_HAL)/platform/banana-pi/*.c) \
    $(WIFI_HOSTAP_SUPPLICANT)/sme.c \
    $(WIFI_HOSTAP_SUPPLICANT)/wmm_ac.c \
    $(WIFI_HOSTAP_SUPPLICANT)/wps_supplicant.c \
    $(WIFI_HOSTAP_SUPPLICANT)/wpas_glue.c \
    $(WIFI_HOSTAP_SUPPLICANT)/interworking.c \
    $(WIFI_HOSTAP_SUPPLICANT)/op_classes.c \
    $(WIFI_HOSTAP_SUPPLICANT)/events.c \
    $(WIFI_HOSTAP_SUPPLICANT)/hs20_supplicant.c \
    $(WIFI_HOSTAP_SUPPLICANT)/scan.c \
    $(WIFI_HOSTAP_SUPPLICANT)/bss.c \
    $(WIFI_HOSTAP_SUPPLICANT)/notify.c \
    $(WIFI_HOSTAP_SUPPLICANT)/wpa_supplicant.c \
    $(WIFI_HOSTAP_SUPPLICANT)/robust_av.c \
    $(WIFI_HOSTAP_SUPPLICANT)/bssid_ignore.c \
    $(WIFI_HOSTAP_SUPPLICANT)/config.c \
    $(WIFI_HOSTAP_SUPPLICANT)/gas_query.c \
    $(WIFI_HOSTAP_SUPPLICANT)/offchannel.c \
    $(WIFI_HOSTAP_SUPPLICANT)/rrm.c \
    $(WIFI_HOSTAP_SUPPLICANT)/config_file.c \
    $(WIFI_HOSTAP_SUPPLICANT)/eap_register.c \
    $(WIFI_HOSTAP_SUPPLICANT)/wnm_sta.c \
    $(WIFI_HOSTAP_SUPPLICANT)/mbo.c \
    $(WIFI_HOSTAP_SUPPLICANT)/wnm_sta.c \
    $(WIFI_HOSTAP_SUPPLICANT)/ctrl_iface.c \
    $(WIFI_HOSTAP_SUPPLICANT)/mbo.c \
    $(WIFI_HOSTAP_SUPPLICANT)/ctrl_iface_unix.c \
    $(WIFI_HOSTAP_SUPPLICANT)/twt.c \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_auth.c \
    $(WIFI_HOSTAP_SRC)/ap/ap_config.c   \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11.c   \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_shared.c   \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_vht.c   \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_he.c   \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_ht.c   \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_1x.c   \
    $(WIFI_HOSTAP_SRC)/ap/ap_mlme.c   \
    $(WIFI_HOSTAP_SRC)/ap/ap_list.c   \
    $(WIFI_HOSTAP_SRC)/ap/sta_info.c   \
    $(WIFI_HOSTAP_SRC)/ap/wnm_ap.c \
    $(WIFI_HOSTAP_SRC)/ap/wpa_auth.c   \
    $(WIFI_HOSTAP_SRC)/ap/wpa_auth_ie.c   \
    $(WIFI_HOSTAP_SRC)/ap/wpa_auth_glue.c   \
    $(WIFI_HOSTAP_SRC)/ap/wps_hostapd.c   \
    $(WIFI_HOSTAP_SRC)/ap/pmksa_cache_auth.c   \
    $(WIFI_HOSTAP_SRC)/ap/eap_user_db.c   \
    $(WIFI_HOSTAP_SRC)/ap/tkip_countermeasures.c   \
    $(WIFI_HOSTAP_SRC)/ap/hostapd.c   \
    $(WIFI_HOSTAP_SRC)/ap/beacon.c   \
    $(WIFI_HOSTAP_SRC)/ap/rrm.c   \
    $(WIFI_HOSTAP_SRC)/ap/wmm.c   \
    $(WIFI_HOSTAP_SRC)/ap/authsrv.c   \
    $(WIFI_HOSTAP_SRC)/ap/bss_load.c   \
    $(WIFI_HOSTAP_SRC)/ap/vlan.c   \
    $(WIFI_HOSTAP_SRC)/ap/vlan_init.c   \
    $(WIFI_HOSTAP_SRC)/ap/vlan_ifconfig.c   \
    $(WIFI_HOSTAP_SRC)/ap/ap_drv_ops.c   \
    $(WIFI_HOSTAP_SRC)/ap/accounting.c   \
    $(WIFI_HOSTAP_SRC)/ap/neighbor_db.c   \
    $(WIFI_HOSTAP_SRC)/ap/drv_callbacks.c   \
    $(WIFI_HOSTAP_SRC)/ap/ctrl_iface_ap.c   \
    $(WIFI_HOSTAP_SRC)/ap/hw_features.c   \
    $(WIFI_HOSTAP_SRC)/ap/dfs.c   \
    $(WIFI_HOSTAP_SRC)/ap/utils.c   \
    $(WIFI_HOSTAP_SRC)/ap/greylist.c   \
    $(WIFI_HOSTAP_SRC)/ap/hs20.c   \
    $(WIFI_HOSTAP_SRC)/ap/gas_serv.c   \
    $(WIFI_HOSTAP_SRC)/ap/acs.c \
    $(WIFI_HOSTAP_SRC)/ap/comeback_token.c \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_eht.c \
    $(WIFI_HOSTAP_SRC)/ap/dhcp_snoop.c \
    $(WIFI_HOSTAP_SRC)/ap/dhcp_snoop.c \
    $(WIFI_HOSTAP_SRC)/ap/eth_p_oui.c \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_eht.c \
    $(WIFI_HOSTAP_SRC)/ap/ieee802_11_eht.c \
    $(WIFI_HOSTAP_SRC)/ap/mbo_ap.c \
    $(WIFI_HOSTAP_SRC)/ap/ndisc_snoop.c \
    $(WIFI_HOSTAP_SRC)/ap/wpa_auth_ft.c \
    $(WIFI_HOSTAP_SRC)/ap/x_snoop.c \
    $(WIFI_HOSTAP_SRC)/ap/scs.c \
    $(WIFI_HOSTAP_SRC)/eapol_auth/eapol_auth_sm.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_methods.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_gtc.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_identity.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_md5.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_mschapv2.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_peap.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_tls_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_tls.c   \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_ttls.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_gpsk.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_ikev2.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_pax.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_psk.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_pwd.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_sake.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_tnc.c \
    $(WIFI_HOSTAP_SRC)/eap_server/eap_server_wsc.c \
    $(WIFI_HOSTAP_SRC)/eap_server/tncs.c \
    $(WIFI_HOSTAP_SRC)/eap_server/ikev2.c \
    $(WIFI_HOSTAP_SRC)/eap_common/chap.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_peap_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_pwd_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_gpsk_common.c \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_pax_common.c \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_psk_common.c \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_sake_common.c \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_ikev2_common.c \
    $(WIFI_HOSTAP_SRC)/eap_common/ikev2_common.c \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_wsc_common.c \
    $(wildcard $(WIFI_HOSTAP_SRC)/eap_peer/*.c) \
    $(WIFI_HOSTAP_SRC)/wps/wps.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_attr_build.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_attr_parse.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_attr_process.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_dev_attr.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_common.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_registrar.c   \
    $(WIFI_HOSTAP_SRC)/wps/wps_enrollee.c   \
    $(WIFI_HOSTAP_SRC)/radius/radius.c   \
    $(WIFI_HOSTAP_SRC)/radius/radius_client.c   \
    $(WIFI_HOSTAP_SRC)/radius/radius_das.c   \
    $(WIFI_HOSTAP_SRC)/utils/wpabuf.c    \
    $(WIFI_HOSTAP_SRC)/utils/wpa_debug.c    \
    $(WIFI_HOSTAP_SRC)/utils/os_unix.c    \
    $(WIFI_HOSTAP_SRC)/utils/eloop.c    \
    $(WIFI_HOSTAP_SRC)/utils/common.c    \
    $(WIFI_HOSTAP_SRC)/utils/ip_addr.c    \
    $(WIFI_HOSTAP_SRC)/utils/uuid.c    \
    $(WIFI_HOSTAP_SRC)/utils/radiotap.c    \
    $(WIFI_HOSTAP_SRC)/utils/crc32.c    \
    $(WIFI_HOSTAP_SRC)/utils/bitfield.c    \
    $(WIFI_HOSTAP_SRC)/utils/config.c    \
    $(WIFI_HOSTAP_SRC)/common/wpa_common.c    \
    $(WIFI_HOSTAP_SRC)/common/sae.c    \
    $(WIFI_HOSTAP_SRC)/common/hw_features_common.c    \
    $(WIFI_HOSTAP_SRC)/common/ieee802_11_common.c    \
    $(WIFI_HOSTAP_SRC)/common/ctrl_iface_common.c    \
    $(WIFI_HOSTAP_SRC)/common/dragonfly.c    \
    $(WIFI_HOSTAP_SRC)/common/gas.c    \
    $(WIFI_HOSTAP_SRC)/common/ptksa_cache.c \
    $(WIFI_HOSTAP_SRC)/crypto/crypto_openssl.c    \
    $(WIFI_HOSTAP_SRC)/crypto/ms_funcs.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-pbkdf2.c    \
    $(WIFI_HOSTAP_SRC)/crypto/random.c    \
    $(WIFI_HOSTAP_SRC)/crypto/tls_internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/dh_groups.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/md5-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-cbc.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha384-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha384-kdf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256-kdf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha512-kdf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha512-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-eax.c \
    $(WIFI_HOSTAP_SRC)/crypto/aes-siv.c \
    $(WIFI_HOSTAP_SRC)/crypto/aes-ctr.c \
    $(WIFI_HOSTAP_SRC)/crypto/aes-encblock.c \
    $(WIFI_HOSTAP_SRC)/l2_packet/l2_packet_linux.c    \
    $(WIFI_HOSTAP_SRC)/rsn_supp/wpa.c    \
    $(WIFI_HOSTAP_SRC)/rsn_supp/wpa_ie.c    \
    $(WIFI_HOSTAP_SRC)/rsn_supp/wpa_ft.c    \
    $(WIFI_HOSTAP_SRC)/rsn_supp/pmksa_cache.c    \
    $(WIFI_HOSTAP_SRC)/rsn_supp/preauth.c    \
    $(WIFI_HOSTAP_SRC)/eapol_supp/eapol_supp_sm.c    \
    $(WIFI_HOSTAP_BASE)/hostapd/ctrl_iface.c    \
    $(WIFI_HOSTAP_BASE)/hostapd/config_file.c    \
    $(WIFI_HOSTAP_BASE)/hostapd/eap_register.c    \
    $(WIFI_HOSTAP_SRC)/drivers/drivers.c    \
    $(WIFI_HOSTAP_SRC)/drivers/driver_common.c    \
    $(WIFI_HOSTAP_SRC)/drivers/driver_nl80211.c    \
    $(WIFI_HOSTAP_SRC)/drivers/driver_nl80211_event.c    \
    $(WIFI_HOSTAP_SRC)/drivers/driver_nl80211_monitor.c    \
    $(WIFI_HOSTAP_SRC)/drivers/driver_nl80211_scan.c    \
    $(WIFI_HOSTAP_SRC)/drivers/driver_nl80211_capa.c    \
    $(WIFI_HOSTAP_SRC)/drivers/linux_ioctl.c    \
    $(WIFI_HOSTAP_SRC)/drivers/netlink.c    \

HAL_LIB_OBJECTS = $(HAL_LIB_SOURCES:.c=.o)  # expands to list of object files
ALL_HAL_LIB_OBJECTS = $(HAL_LIB_OBJECTS)
HAL_LIB_FLAGS = -g -fPIC $(INCLUDE_HAL_LIB_DIRS) -DBANANA_PI_PORT -D_PLATFORM_BANANAPI_R4_ -DCONFIG_HW_CAPABILITIES -DEM_APP -DCONFIG_WIFI_EMULATOR_EXT_AGENT
HOSTAP_LIB_FLAGS = -g -fPIC $(INCLUDE_HOSTAP_LIB_DIRS) -DHOSTAPD -DCONFIG_CRYPTO_INTERNAL -DCONFIG_DRIVER_NL80211 -DNEED_AP_MLME -DCONFIG_IEEE80211AC -DCONFIG_IEEE80211AX -DCONFIG_IEEE80211N -DCONFIG_WPS -DIEEE8021X_EAPOL -DEAP_SERVER_IDENTITY -DEAP_SERVER_MD5 -DEAP_SERVER_TLS -DEAP_SERVER_MSCHAPV2 -DEAP_SERVER_PEAP -DEAP_SERVER_TTLS -DCONFIG_ECC -DRDK_ONEWIFI -DCONFIG_IEEE80211W -DCONFIG_OPENSSL_CMAC -DCONFIG_LIBNL20 -DCONFIG_WNM_AP
HOSTAP_LIB_FLAGS +=  -DHOSTAPD_2_11 -DCONFIG_WEP -DFEATURE_SUPPORT_RADIUSGREYLIST -DCONFIG_SAE -DCONFIG_SUITEB192 -DCONFIG_SUITE -DCONFIG_SUITEB -DCONFIG_CRYPTO_INTERNAL -DCONFIG_SHA256 -DCONFIG_SHA384 -DCONFIG_SHA512 -DCONFIG_HMAC_SHA384_KDF -DCONFIG_SME -DCONFIG_INTERWORKING -DCONFIG_GAS -DCONFIG_HS20 \
                         -DCONFIG_TESTING_OPTIONS -DHOSTAPD_DUMP_STATE -DCONFIG_CTRL_IFACE_UNIX -DCONFIG_CTRL_IFACE -DCONFIG_CTRL_IFACE_MIB -DCONFIG_IEEE80211R -DCONFIG_IEEE80211R_AP -DCONFIG_ETH_P_OUI -DCONFIG_WNM -DCONFIG_WNM_AP -DCONFIG_IEEE80211AC -DCONFIG_IEEE80211BE -DCONFIG_IEEE80211AX -DCONFIG_MBO \
                         -DNO_SUPPLICANT -DCONFIG_DRIVER_NL80211  -DEAP_SERVER_MD5 -DEAP_SERVER_TLS -DEAP_SERVER_UNAUTH_TLS -DEAP_SERVER_PEAP -DEAP_SERVER_TTLS -DEAP_SERVER_MSCHAPV2 -DEAP_SERVER_GTC -DEAP_SERVER_PAX -DEAP_SERVER_PSK -DEAP_SERVER_SAKE -DEAP_SERVER_GPSK -DEAP_GPSK_SHA256 -DEAP_SERVER_PWD \
                         -DCONFIG_WPS -DEAP_SERVER_WSC -DCONFIG_WPS_TESTING -DEAP_SERVER_IKEV2 -DEAP_SERVER_TNC -DEAP_SERVER_IDENTITY -DEAP_SERVER -DPKCS12_FUNCS -DEAP_TLS_FUNCS -DCONFIG_TLSV11 -DCONFIG_TLSV12 -DCONFIG_TLS_OPENSSL -DCRYPTO_RSA_OAEP_SHA256 -DTLS_DEFAULT_CIPHERS=\""DEFAULT:!EXP:!LOW"\" -DCONFIG_DES \
                         -DCONFIG_HMAC_SHA256_KDF -DALL_DH_GROUPS -DCONFIG_ECC -DCONFIG_IPV6 -DNEED_AP_MLME -DCONFIG_PROXYARP -DCONFIG_ACS -DCONFIG_DEBUG_LINUX_TRACING -DCONFIG_DEBUG_FILE -DEAP_TLS -DEAP_UNAUTH_TLS -DEAP_PEAP -DEAP_TTLS -DEAP_MD5 -DEAP_MSCHAPv2 -DEAP_GTC -DEAP_OTP -DEAP_LEAP -DEAP_PSK -DEAP_PAX \
                         -DEAP_SAKE -DEAP_GPSK -DEAP_PWD -DIEEE8021X_EAPOL -DCONFIG_IEEE80211W -DRDK_ONEWIFI -DCONFIG_GENERIC_MLO -DCONFIG_MLO -fPIC -DPIC
INCLUDEDIRS = \
        -I$(WIFI_HAL_INTERFACE) \
	-I$(WIFI_CJSON)/ \
	-I$(WIFI_HOSTAP_SRC) \
	-I$(WLAN_EMU_HOME)/inc \
	-I$(WLAN_EMU_HOME)/src/utils \
	-I$(WLAN_EMU_HOME)/src/bus/openwrt \
	-I$(ONEWIFI_HOME)/source/utils \
	-I$(ONEWIFI_HOME)/include \
	-I$(ONEWIFI_BUS_LIB_HOME)/inc \
	-I$(ONEWIFI_HOME)/lib/log \
	-I$(ONEWIFI_HOME)/lib/ds \
	-I$(ONEWIFI_HOME)/source/platform/linux \
	-I$(ONEWIFI_HOME)/source/platform/common \
	-I$(WLAN_EMU_HOME)/src/util_crypto \
	-I$(WIFI_DBUS)/ \
	-I$(ONEWIFI_HOME)/source/platform/linux/he_bus/inc \
	-I$(ONEWIFI_HOME)/source/ccsp \
	-I/usr/include/libnl3 \
	-I/usr/local/ssl/include/ \
	-I$(WIFI_NETLINK)/include \
	-I$(ONEWIFI_HOME)/lib/json_util \
	-I$(WLAN_EMU_SRC)/external_agent_cci/ext_inc

CXXFLAGS += $(INCLUDEDIRS) -g -Wall -Wextra -Wpointer-arith -Wcast-qual -Wcast-align -Wstrict-aliasing -fno-common -Wctor-dtor-privacy -Wold-style-cast -Woverloaded-virtual -Wsign-promo -Wstrict-null-sentinel -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE -pie -ftrapv -Wformat=2 -Wformat-security -Wuninitialized -Winit-self -Wconversion -Wsign-conversion -Weffc++ -Wno-unused-parameter -std=c++17 -fpermissive -DCONFIG_EXT_AGENT_CCI
CXXFLAGS += -fPIC -D_ANSC_LINUX -D_COSA_INTEL_USG_ATOM_ -DUSE_NOTIFY_COMPONENT -DCISCO_XB3_PLATFORM_CHANGES -DDUAL_CORE_XB3 -DFEATURE_ONE_WIFI -DWIFI_HAL_VERSION_3 -DFEATURE_SUPPORT_PASSPOINT -DFEATURE_SUPPORT_WEBCONFIG -DBANANA_PI_PORT  -DNL80211_ACL -D_PLATFORM_BANANAPI_R4_ -DFEATURE_SINGLE_PHY -DEM_APP -DONEWIFI_STA_MGR_APP_SUPPORT 

ifneq ($(OS), Darwin)
CXXFLAGS += -DPLATFORM_LINUX
else
CXXFLAGS += -DPLATFORM_OSX
endif

LIBDIRS = \
	-L$(INSTALLDIR)/lib \
	-L$(ONEWIFI_HOME)/install/lib/ \
	-L/usr/local/lib    \
	-L/usr/local/ssl/lib/ \
	-L/usr/lib/ \

LIBS = -lgcc -lc -lm -lpthread -ldl -L$(WIFI_OPENSSL_DIR) -lssl -L$(WIFI_OPENSSL_DIR) -lcrypto -lnl-3 -lnl-genl-3 -lnl-route-3 -lcjson -luuid -lwebconfig -lnl-3 -lnl-genl-3 -lnl-route-3 -lpthread -lev -lstdc++fs -lwifihalcci -lpcap -lcurl

LDFLAGS += $(LIBDIRS) $(LIBS)

EXCLUDE = $(WLAN_EMU_SRC)/utils/collection.c
GENERIC_SOURCES = $(filter-out $(EXCLUDE), $(wildcard $(WLAN_EMU_SRC)/utils/*.c)) \
		  $(ONEWIFI_HOME)/lib/common/util.c

FRAMEWORK_SOURCES = $(wildcard $(WLAN_EMU_SRC)/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/sta/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/messages/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/tests/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/ui/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/utils/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/common/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/external_agent_cci/*.cpp) \

CCI_HDRS = $(wildcard $(WLAN_EMU_SRC)/utils/*.hpp)

CCI_OBJECTS = $(FRAMEWORK_SOURCES:.cpp=.o)
GENERIC_OBJECTS = $(GENERIC_SOURCES:.c=.o) 
ALLOBJECTS = $(CCI_OBJECTS) $(GENERIC_OBJECTS)

$(BUILD_DIR):
	@mkdir -p $(INSTALLDIR)/lib
	@mkdir -p $(INSTALLDIR)/bin

all: $(BUILD_DIR)  $(HAL_LIBRARY) $(WEBCONFIG_LIBRARY) $(PROGRAM)

$(HAL_LIBRARY): $(ALL_HAL_LIB_OBJECTS)
	$(AR) -rcs $@  $^

$(PROGRAM): $(ALLOBJECTS)
	$(CXX) -o $@ $(ALLOBJECTS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CXXFLAGS) $(HAL_LIB_FLAGS) $(HOSTAP_LIB_FLAGS) -o $@ -c $<

$(CCI_OBJECTS): %.o: %.cpp $(CCI_HDRS)
	$(CXX) $(CXXFLAGS) -DCONFIG_WIFI_EMULATOR_EXT_AGENT -o $@ -c $<

# Clean target: "make -f Makefile.Linux clean" to remove unwanted objects and executables.
#

clean:
	$(RM) $(ALLOBJECTS) $(ALL_HAL_LIB_OBJECTS) $(HAL_LIBRARY) $(PROGRAM)

#
# Run target: "make -f Makefile.Linux run" to execute the application
#             You will need to add $(VARIABLE_NAME) for any command line parameters 
#             that you defined earlier in this file.
# 

run:
	./$(PROGRAM) 
