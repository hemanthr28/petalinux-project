#
# This file is the nalla250socbist recipe.
#

SUMMARY = "Simple nalla250socbist application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://ep_daemon.sh \
	  file://nalla250socd \
	  file://250_soc_bist \
	  file://run.sh \
	  file://250_soc_bist_config \
	  file://clocks_test_bist_config \
	  file://parallel_test_bist_config \
	  file://libnalla250_soc_lib.so \
	"

S = "${WORKDIR}"

# Added these 2 lines to avoid bitbake complaining about the bist exe & libnalla250_soc_lib.so
INSANE_SKIP_${PN} = "ldflags"
INSANE_SKIP_${PN}-dev = "ldflags"

do_install() {
	     install -d ${D}/${datadir}/nallatech
	     install -d ${D}/${datadir}/nallatech/bist
	     install -d ${D}/${datadir}/nallatech/bist/bin
	     install -d ${D}/${datadir}/nallatech/bist/bist_configs
	     install -d ${D}/${datadir}/nallatech/bist/bin/arm
	     install -d ${D}/${sysconfdir}/rc3.d
	     install -d ${D}/${sysconfdir}/rc4.d
	     install -d ${D}/${sysconfdir}/rc5.d
	     install -d ${D}/${sysconfdir}/init.d
	     install -m 0755 ${S}/ep_daemon.sh ${D}/${datadir}/nallatech/
	     install -m 0755 ${S}/250_soc_bist ${D}/${datadir}/nallatech/bist/bin/arm
	     install -m 0755 ${S}/run.sh ${D}/${datadir}/nallatech/bist/bin/arm
	     install -m 0755 ${S}/250_soc_bist_config ${D}/${datadir}/nallatech/bist/bist_configs
	     install -m 0755 ${S}/clocks_test_bist_config ${D}/${datadir}/nallatech/bist/bist_configs
	     install -m 0755 ${S}/parallel_test_bist_config ${D}/${datadir}/nallatech/bist/bist_configs
	     install -m 0755 ${S}/libnalla250_soc_lib.so ${D}/${datadir}/nallatech/bist/bin/arm
	     install -m 0755 ${S}/nalla250socd ${D}/${sysconfdir}/init.d/
	     cd ${D}/${sysconfdir}/rc3.d
	     ln -s ../init.d/nalla250socd S90nalla250socd
	     cd ${D}/${sysconfdir}/rc4.d
	     ln -s ../init.d/nalla250socd S90nalla250socd
	     cd ${D}/${sysconfdir}/rc5.d
	     ln -s ../init.d/nalla250socd S90nalla250socd
}

FILES_${PN} = "${datadir}/nallatech/ \
	      ${sysconfdir}/ \
	      ${libdir}/ \
"

