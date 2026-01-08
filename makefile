include ./makefile.inc

LDFLAGS = $(LIBDIRS) $(LIBS)
HAL_LIBRARY = $(INSTALLDIR)/lib/libwifihal.so

HAL_INCLUDE_DIRS = \
    -I$(ONEWIFI_HOME)/one-wifi/generic/include \
    -I$(WIFI_CJSON)/ \
    -I$(WIFI_RDK_HAL)/src \
    -I$(WIFI_RDK_HAL)/util_crypto \
    -I$(WIFI_DEVICE_HAL)/ \
    -I$(WIFI_LIB_PCAP)/ \
    -I$(WIFI_OPENSSL_DIR)/include \
    -I$(ONEWIFI_HOME)/halinterface/generic \
    -I/usr/include/libnl3 \

HOSTAP_INCLUDE_DIRS = \
    -I$(WIFI_HOSTAP_BASE) \
    -I$(WIFI_HOSTAP_SRC) \
    -I$(WIFI_HOSTAP_SRC)/ap \
    -I$(WIFI_HOSTAP_SRC)/utils \
    -I$(WIFI_HOSTAP_SRC)/drivers \

HAL_LIB_SOURCES = $(wildcard $(WIFI_RDK_HAL)/src/*.c) \
    $(wildcard $(WIFI_RDK_HAL)/platform/emulator/*.c) \
    $(wildcard $(WIFI_DEVICE_HAL)/*.c) \
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
    $(WIFI_HOSTAP_SRC)/eap_common/chap.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_eke_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_fast_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_gpsk_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_ikev2_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_pax_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_peap_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_psk_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_pwd_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_sake_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_sim_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_teap_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/eap_wsc_common.c   \
    $(WIFI_HOSTAP_SRC)/eap_common/ikev2_common.c   \
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
    $(WIFI_HOSTAP_SRC)/utils/base64.c    \
    $(WIFI_HOSTAP_SRC)/utils/wpabuf.c    \
    $(WIFI_HOSTAP_SRC)/utils/wpa_debug.c    \
    $(WIFI_HOSTAP_SRC)/utils/os_unix.c    \
    $(WIFI_HOSTAP_SRC)/utils/eloop.c    \
    $(WIFI_HOSTAP_SRC)/utils/common.c    \
    $(WIFI_HOSTAP_SRC)/utils/ip_addr.c    \
    $(WIFI_HOSTAP_SRC)/utils/uuid.c    \
    $(WIFI_HOSTAP_SRC)/utils/radiotap.c    \
    $(WIFI_HOSTAP_SRC)/common/wpa_common.c    \
    $(WIFI_HOSTAP_SRC)/common/hw_features_common.c    \
    $(WIFI_HOSTAP_SRC)/common/ieee802_11_common.c    \
    $(WIFI_HOSTAP_SRC)/common/ctrl_iface_common.c    \
    $(WIFI_HOSTAP_SRC)/common/dragonfly.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-wrap.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-unwrap.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-ctr.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-encblock.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-eax.c    \
    $(WIFI_HOSTAP_SRC)/crypto/fips_prf_openssl.c    \
    $(WIFI_HOSTAP_SRC)/crypto/ms_funcs.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-pbkdf2.c    \
    $(WIFI_HOSTAP_SRC)/crypto/random.c    \
    $(WIFI_HOSTAP_SRC)/crypto/dh_groups.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha256-tlsprf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-cbc.c    \
    $(WIFI_HOSTAP_SRC)/crypto/aes-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/crypto_openssl.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/md5-internal.c    \
    $(WIFI_HOSTAP_SRC)/crypto/md5.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha384-prf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/sha1-tprf.c    \
    $(WIFI_HOSTAP_SRC)/crypto/tls_openssl.c    \
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


HAL_LIB_OBJECTS = $(HAL_LIB_SOURCES:.c=.o)
HAL_LIB_FLAGS = -g -fPIC $(HAL_INCLUDE_DIRS) -DENABLE_FEATURE_MESHWIFI -DWLAN_EMULATOR -DWIFI_HAL_VERSION_3
HOSTAP_LIB_FLAGS = -g -fPIC $(HOSTAP_INCLUDE_DIRS) -DHOSTAPD -DCONFIG_DRIVER_NL80211 -DNEED_AP_MLME -DCONFIG_IEEE80211AC -DCONFIG_IEEE80211AX -DCONFIG_IEEE80211N -DCONFIG_WPS -DIEEE8021X_EAPOL -DEAP_SERVER_IDENTITY -DEAP_SERVER_TLS -DEAP_SERVER_MSCHAPV2 -DEAP_SERVER_PEAP -DEAP_SERVER_TTLS -DCONFIG_ECC -DRDK_ONEWIFI -DCONFIG_IEEE80211W -DCONFIG_OPENSSL_CMAC -DCONFIG_FIPS -DNO_SHA256_WRAPPER -DCONFIG_SHA384 -DEAP_FAST_OR_TEAP


#
# program
#

PROGRAM = $(INSTALLDIR)/bin/wlan_emu

INCLUDEDIRS = \
    -I$(WLAN_EMU_HOME)/src/bus/rdkb \
    -I$(WLAN_EMU_HOME)/src/bus/common \
    -I$(WLAN_EMU_HOME)/inc \
    -I$(WLAN_EMU_HOME)/src/utils \
    -I$(ONEWIFI_HOME)/halinterface/generic \
    -I/usr/include/libnl3 \

CXXFLAGS = $(INCLUDEDIRS) -g
LDFLAGS = $(LIBDIRS) $(LIBS)
LIBDIRS = \
    -L$(INSTALLDIR)/lib \

LIBS = -lm -lpthread -ldl -lwifihal -L$(WIFI_OPENSSL_DIR) -lssl -L$(WIFI_OPENSSL_DIR) -lcrypto -lnl-3 -lnl-genl-3 -lnl-route-3 -lcurl -lcjson

GENERIC_SOURCES = $(wildcard $(WLAN_EMU_SRC)/utils/*.c) \

FRAMEWORK_SOURCES = $(wildcard $(WLAN_EMU_SRC)/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/radio/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/vap/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/sta/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/messages/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/tests/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/ui/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/utils/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/bus/common/*.cpp) \
    $(wildcard $(WLAN_EMU_SRC)/bus/rdkb/*.cpp) \

FRAMEWORK_OBJECTS = $(FRAMEWORK_SOURCES:.cpp=.o)
GENERIC_OBJECTS = $(GENERIC_SOURCES:.c=.o)
ALLOBJECTS = $(FRAMEWORK_OBJECTS) $(GENERIC_OBJECTS)

all: $(HAL_LIBRARY) $(PROGRAM)

$(HAL_LIBRARY): $(HAL_LIB_OBJECTS)
	$(CXX) -shared -fPIC -o $@ $^


$(PROGRAM): $(ALLOBJECTS)
	$(CXX) -o $@ $(ALLOBJECTS) $(LDFLAGS)

$(HAL_LIB_OBJECTS): %.o: %.c
	$(CC) $(HAL_LIB_FLAGS) $(HOSTAP_LIB_FLAGS) -o $@ -c $<

$(GENERIC_OBJECTS): %.o: %.c
	$(CC) $(CXXFLAGS) -o $@ -c $<

$(FRAMEWORK_OBJECTS): %.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

# Clean target: "make -f Makefile.Linux clean" to remove unwanted objects and executables.
#

clean:
	$(RM) $(HAL_LIB_OBJECTS) $(HAL_LIBRARY) $(ALLOBJECTS) $(PROGRAM)

#
# Run target: "make -f Makefile.Linux run" to execute the application
#             You will need to add $(VARIABLE_NAME) for any command line parameters
#             that you defined earlier in this file.
#

#run:
#    ./$(PROGRAM)
