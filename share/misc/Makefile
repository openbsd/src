#	$OpenBSD: Makefile,v 1.4 2000/03/02 14:46:51 todd Exp $
#	from: @(#)Makefile	5.13 (Berkeley) 5/7/91

FILES=	airport ascii birthtoken eqnchar getopt inter.phone man.template \
	mdoc.template na.phone operator scsi_modes zipcodes
NOOBJ=	noobj

all clean cleandir depend lint tags:

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
