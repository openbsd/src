# $FreeBSD: src/gnu/usr.bin/cc/cc1obj/Makefile,v 1.28.8.1 2009/04/15 03:14:26 kensmith Exp $
# $OpenBSD: Makefile,v 1.2 2010/05/06 20:58:10 naddy Exp $

.include <bsd.own.mk>

.include "${.CURDIR}/../Makefile.inc"
.include "${.CURDIR}/../Makefile.ver"

.PATH: ${GCCDIR}/objc ${GCCDIR}

PROG=	cc1obj
SRCS=	main.c c-parser.c objc-act.c objc-lang.c c-decl.c
BINDIR= /usr/lib/gcc-lib/${GCC_TARGET}/${BASEVER}
NOMAN=	Yes
NO_PIC=	Yes

CFLAGS+= -I${GCCDIR}/objc -I.

OBJS+=  ${PROG}-checksum.o
DPADD=	${LIBBACKEND} ${LIBCPP} ${LIBDECNUMBER} ${LIBIBERTY}
LDADD=	${LIBBACKEND} ${LIBCPP} ${LIBDECNUMBER} ${LIBIBERTY}

DOBJS+=	${SRCS:N*.h:R:S/$/.o/g}
${PROG}-dummy:	${DOBJS}
	${CC} ${CFLAGS} ${LDFLAGS} -o ${.TARGET} ${DOBJS} ${LDADD}
CLEANFILES+=	${PROG}-dummy

${PROG}-checksum.c:	${PROG}-dummy
	../cc_tools/genchecksum ${PROG}-dummy > ${.TARGET}
CLEANFILES+=	${PROG}-checksum.c

.include <bsd.prog.mk>
