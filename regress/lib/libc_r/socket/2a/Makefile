# $OpenBSD: Makefile,v 1.4 2002/09/02 20:01:43 avsm Exp $

PROG=	socket2a
CFLAGS+= -I${.CURDIR}/../../include

REGRESS_TARGETS=dummy

dummy: ${PROG}
	@cd ${.CURDIR}/../2; \
	if test -d ${__objdir} ; then \
		cd ${__objdir} ; \
	fi; \
	ln -sf ${.OBJDIR}/${PROG}
	# Nothing here so far...

.include <bsd.regress.mk>

