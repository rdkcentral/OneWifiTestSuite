#obj-$(CONFIG_RDKFMAC) += rdkfmac.o
obj-m += rdkfmac.o
rdkfmac-objs += \
    rdkfmac_netdev.o \
    rdkfmac_cdev.o \
    rdkfmac_bus.o \
    rdkfmac_cmd.o \

KVERSION = $(shell uname -r)
all:
    make -C /lib/modules/$(KVERSION)/build M=$(PWD) modules
clean:
    make -C /lib/modules/$(KVERSION)/build M=$(PWD) clean
