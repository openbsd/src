#	$OpenBSD: Makefile,v 1.21 2011/10/06 20:49:28 deraadt Exp $

LDSTATIC=${STATIC}
PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace -I${.CURDIR} -I${.CURDIR}/../../sys
SRCS=	kdump.c ioctl.c kdump_subr.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c kdump_subr.c

ioctl.c: ${.CURDIR}/Makefile ${.CURDIR}/mkioctls
	CC="${CC}" \
	/bin/sh ${.CURDIR}/mkioctls /usr/include/sys/ioctl.h \
	    /usr/include/sys/ioctl_compat.h \
	    /usr/include/crypto/cryptodev.h \
	    /usr/include/dev/biovar.h \
	    /usr/include/dev/systrace.h \
	    /usr/include/dev/wscons/wsconsio.h \
	    /usr/include/dev/vndioctl.h \
	    /usr/include/net/bpf.h \
	    /usr/include/net/if_ppp.h \
	    /usr/include/net/if_pppoe.h \
	    /usr/include/net/if_tun.h \
	    /usr/include/net/pfvar.h \
	    /usr/include/net80211/ieee80211_ioctl.h \
	    /usr/include/netinet6/in6_var.h \
	    /usr/include/sys/agpio.h \
	    /usr/include/sys/ataio.h \
	    /usr/include/sys/audioio.h \
	    /usr/include/sys/cdio.h \
	    /usr/include/sys/chio.h \
	    /usr/include/sys/dkio.h \
	    /usr/include/sys/filio.h \
	    /usr/include/sys/gpio.h \
	    /usr/include/sys/midiio.h \
	    /usr/include/sys/mtio.h \
	    /usr/include/sys/pciio.h \
	    /usr/include/sys/radioio.h \
	    /usr/include/sys/scanio.h \
	    /usr/include/sys/scsiio.h \
	    /usr/include/sys/sockio.h > ioctl.c

kdump_subr.c: ${.CURDIR}/mksubr
	sh ${.CURDIR}/mksubr /usr/include > ${.TARGET}

.include <bsd.prog.mk>
