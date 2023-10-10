ccflags-y       += -I$(src)/../../../inc
obj-m += rdkfmac.o
rdkfmac-objs += \
    rdkfmac_netdev.o \
    rdkfmac_mac80211.o \
    rdkfmac_cdev.o \
    rdkfmac_bus.o \
    rdkfmac_cmd.o \
    rdkfmac_cfg80211.o \
    scan.o \
    util.o \
    chan.o \
    mlme.o trace.o \
    tx.o \
    sta_info.o \
    iface.o \
    driver-ops.o \
    rate.o \
    key.o \
    aes_cmac.o \
    aes_gmac.o \
    led.o \
    wep.o \
    fils_aead.o \
    wpa.o \
    tkip.o\
    wme.o \
    mesh.o \
    mesh_pathtbl.o \
    mesh_plink.o \
    mesh_hwmp.o \
    mesh_sync.o \
    mesh_ps.o \
    aead_api.o \
    michael.o \
    rx.o \
    ibss.o \
    ht.o agg-tx.o agg-rx.o \
    vht.o \
    he.o \
    tdls.o \
    offchannel.o \
    ocb.o \
    eht.o \
    spectmgmt.o \
    pm.o \
    s1g.o \
    status.o \
    ethtool.o \
    airtime.o \

KVERSION = $(shell uname -r)
all:
    make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
    make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
