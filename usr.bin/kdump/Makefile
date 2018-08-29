#	$OpenBSD: Makefile,v 1.35 2018/08/29 17:47:07 anton Exp $

LDSTATIC=${STATIC}
PROG=	kdump
SYS_DIR=${.CURDIR}/../../sys
CFLAGS+=-I${.CURDIR}/../ktrace -I${.CURDIR} -I${SYS_DIR}
SRCS=	kdump.c ioctl.c kdump_subr.c subr.c ktrstruct.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c kdump_subr.c

ioctl.c: ${.CURDIR}/Makefile ${.CURDIR}/mkioctls
	CC="${CC}" \
	/bin/sh ${.CURDIR}/mkioctls ${SYS_DIR}/sys/ioctl.h \
	    ${SYS_DIR}/dev/biovar.h \
	    ${SYS_DIR}/dev/ipmi.h \
	    ${SYS_DIR}/dev/wscons/wsconsio.h \
	    ${SYS_DIR}/dev/vndioctl.h \
	    ${SYS_DIR}/dev/vscsivar.h \
	    ${SYS_DIR}/dev/pci/drm/i915_drm.h \
	    ${SYS_DIR}/dev/pv/pvvar.h \
	    ${SYS_DIR}/dev/usb/udlio.h \
	    ${SYS_DIR}/dev/usb/usb.h \
	    ${SYS_DIR}/miscfs/fuse/fusefs.h \
	    ${SYS_DIR}/net/bpf.h \
	    ${SYS_DIR}/net/if_ppp.h \
	    ${SYS_DIR}/net/if_pppoe.h \
	    ${SYS_DIR}/net/if_trunk.h \
	    ${SYS_DIR}/net/if_tun.h \
	    ${SYS_DIR}/net/pfvar.h \
	    ${SYS_DIR}/net/pipex.h \
	    ${SYS_DIR}/net80211/ieee80211_ioctl.h \
	    ${SYS_DIR}/netinet6/in6_var.h \
	    ${SYS_DIR}/sys/tty.h \
	    ${SYS_DIR}/sys/syslog.h \
	    ${SYS_DIR}/sys/ataio.h \
	    ${SYS_DIR}/sys/audioio.h \
	    ${SYS_DIR}/sys/cdio.h \
	    ${SYS_DIR}/sys/chio.h \
	    ${SYS_DIR}/sys/dkio.h \
	    ${SYS_DIR}/sys/filio.h \
	    ${SYS_DIR}/sys/gpio.h \
	    ${SYS_DIR}/sys/kcov.h \
	    ${SYS_DIR}/sys/memrange.h \
	    ${SYS_DIR}/sys/mtio.h \
	    ${SYS_DIR}/sys/pciio.h \
	    ${SYS_DIR}/sys/radioio.h \
	    ${SYS_DIR}/sys/scsiio.h \
	    ${SYS_DIR}/sys/sockio.h \
	    ${SYS_DIR}/sys/videoio.h \
		> ioctl.c

kdump_subr.c: ${.CURDIR}/mksubr
	sh ${.CURDIR}/mksubr ${SYS_DIR} > ${.TARGET}

.include <bsd.prog.mk>
