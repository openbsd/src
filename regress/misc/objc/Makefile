#	$OpenBSD: Makefile,v 1.4 2013/10/06 20:05:30 guenther Exp $

.SUFFIXES: .m

COMPILE.m?=	${CC} ${CFLAGS} ${CPPFLAGS} -c
LINK.m?=	${CC} ${CFLAGS} ${CPPFLAGS} ${LDFLAGS}

.m:
	${LINK.m} -o ${.TARGET} ${.IMPSRC} ${LDLIBS}
.m.o:
	${COMPILE.m} ${.IMPSRC}

PROG=	main
SRCS=	main.m
LDADD=	-lobjc -lpthread

.include <bsd.regress.mk>
