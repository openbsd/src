#	$OpenBSD: Makefile,v 1.5 2001/03/02 01:03:23 mickey Exp $
#	from: @(#)Makefile	5.13 (Berkeley) 5/7/91

FILES=	airport ascii birthtoken eqnchar getopt inter.phone man.template \
	mdoc.template na.phone operator scsi_modes zipcodes
SUBDIR=	pcvtfonts

all clean cleandir depend lint obj tags: _SUBDIRUSE

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
