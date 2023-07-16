FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://devtool-fragment.cfg"

#FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
#SRC_URI += "file://popcorn.cfg"

#Newly added for popcorn 
KERNEL_VERSION_SANITY_SKIP="1"