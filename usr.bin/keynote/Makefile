#	$OpenBSD: Makefile,v 1.7 1999/10/01 01:08:29 angelos Exp $

all:
	@cd ${.CURDIR}/../../lib && ${MAKE} libkeynote

install:
	@cd ${.CURDIR}/../../lib/libkeynote && ${MAKE} -f Makefile.bsd-wrapper install-keynote

clean:
	@cd ${.CURDIR}/../../lib/libkeynote && ${MAKE} -f Makefile.bsd-wrapper clean

cleandir:
	@cd ${.CURDIR}/../../lib/libkeynote && ${MAKE} -f Makefile.bsd-wrapper cleandir

.include <bsd.prog.mk>
