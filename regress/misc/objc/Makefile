#	$OpenBSD: Makefile,v 1.2 2005/04/18 17:45:46 miod Exp $

.SUFFIXES: .m

COMPILE.m?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.m?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}

.m:
	${LINK.m} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.m.o:
	${COMPILE.m} ${.IMPSRC}

PROG=	main
SRCS=	main.m
LDADD=	-lobjc

.include <bsd.regress.mk>
