SUMMARY = "Recipe for  build an external nalla250soc Linux kernel module"
SECTION = "PETALINUX/modules"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

INHIBIT_PACKAGE_STRIP = "1"

SRC_URI = "file://Makefile \
           file://nalla_250_soc_device.c \
           file://nalla_250_soc_device.h \
           file://nalla_250_soc_end_point.c \
           file://nalla_250_soc_end_point.h \
           file://nalla_250_soc_ep_network.c \
           file://nalla_250_soc_ep_network.h \
	   file://COPYING \
          "

S = "${WORKDIR}"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
