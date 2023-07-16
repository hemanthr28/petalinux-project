FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRC_URI += "file://0001-FSBL.patch"
  
  
#do_compile_prepend(){

#   install -m 0644 ${TOPDIR}/../project-spec/hw-description/psu_init.c ${B}/fsbl/psu_init.c

#   install -m 0644 ${TOPDIR}/../project-spec/hw-description/psu_init.h ${B}/fsbl/psu_init.h

#}
