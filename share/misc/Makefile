#	from: @(#)Makefile	5.13 (Berkeley) 5/7/91
#	$Id: Makefile,v 1.2 1996/06/12 11:21:45 deraadt Exp $

FILES=	airport ascii birthtoken eqnchar getopt inter.phone man.template \
	mdoc.template na.phone operator scsi_modes style zipcodes
NOOBJ=	noobj

all clean cleandir depend lint tags:

realinstall:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/misc

.include <bsd.prog.mk>
