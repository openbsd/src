# $OpenBSD: Makefile,v 1.2 2002/01/03 01:43:24 art Exp $

PROG=	socket2a
SRCS=	socket2a.c
CFLAGS+= -I${.CURDIR}/../../include

regress: ${PROG}
	@cd ${.CURDIR}/../2; \
	if test -d ${__objdir} ; then \
		cd ${__objdir} ; \
	fi; \
	ln -sf ${.OBJDIR}/${PROG}
	# Nothing here so far...

.include <bsd.prog.mk>
