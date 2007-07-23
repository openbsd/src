#	$OpenBSD: Makefile,v 1.3 2007/07/23 17:24:06 kurt Exp $

.SUFFIXES: .m

COMPILE.m?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.m?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}

.m:
	${LINK.m} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.m.o:
	${COMPILE.m} ${.IMPSRC}

PROG=	main
SRCS=	main.m
LDADD=	-lobjc -pthread

.include <bsd.regress.mk>
