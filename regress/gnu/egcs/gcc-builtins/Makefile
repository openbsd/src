# $OpenBSD: Makefile,v 1.3 2016/09/25 21:35:12 bluhm Exp $

GCC_BUILTINS=	sprintf-1 sprintf-2 sprintf-3 stpcpy-1 stpcpy-2 strcat-1 \
		strcat-2 strcpy-1 strcpy-2 strncat-1 vsprintf-1 vsprintf-2 \
		vsprintf-3

TCC=		gcc
TCFLAGS=	-O2
TCXXFLAGS=	${TCFLAGS}

.for i in ${GCC_BUILTINS}
REGRESS_TARGETS+= c-${i}
c-${i}:
	${TCC} ${TCFLAGS} -o /dev/null ${i}.c 2>&1 | sed '/^\/tmp\//d;s/(\.text+0x[0-9a-f]*)//' | diff -u - ${.CURDIR}/${i}.c.exp
.endfor

generate:
.for i in ${REGRESS_TARGETS}
	-${TCC} ${TCFLAGS} -o /dev/null ${i:C/^c-//g}.c 2>&1 | sed '/^\/tmp\//d;s/(\.text+0x[0-9a-f]*)//' >${i:C/^c-//g}.c.exp
.endfor

.PHONY:	${REGRESS_TARGETS} generate regress

NOOBJ= Yes

.include <bsd.regress.mk>
