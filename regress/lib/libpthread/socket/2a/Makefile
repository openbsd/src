# $OpenBSD: Makefile,v 1.1.1.1 2001/08/15 14:37:10 fgsch Exp $

PROG=	socket2a
SRCS=	socket2a.c
CFLAGS+= -I${.CURDIR}/../../include

regress:
	@cd ${.CURDIR}/../2; \
	if test -d ${__objdir} ; then \
		cd ${__objdir} ; \
	fi; \
	ln -sf ${.OBJDIR}/${PROG}
	# Nothing here so far...

.include <bsd.prog.mk>
