#	$OpenBSD: Makefile,v 1.8 2001/10/05 17:30:51 matthieu Exp $
#	from: @(#)Makefile	5.13 (Berkeley) 5/7/91

FILES=	airport ascii birthtoken countrycodes eqnchar getopt \
	inter.phone man.template mdoc.template na.phone operator \
	scsi_modes usb_hid_usages usb_hid_usages zipcodes
.if ${MACHINE} == "i386"
SUBDIR=	pcvtfonts
.endif

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
