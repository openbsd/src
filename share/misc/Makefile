#	$OpenBSD: Makefile,v 1.11 2007/08/01 21:31:45 deraadt Exp $
#	from: @(#)Makefile	5.13 (Berkeley) 5/7/91

FILES=	airport ascii birthtoken countrycodes eqnchar getopt \
	inter.phone license.template man.template mdoc.template \
	na.phone operator scsi_modes usb_hid_usages usb_hid_usages \
	zipcodes 

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64") || \
    (${MACHINE} == "alpha")
SUBDIR=	pcvtfonts
.endif

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
