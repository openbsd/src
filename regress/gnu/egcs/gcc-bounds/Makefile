# $OpenBSD: Makefile,v 1.5 2004/04/23 21:36:14 avsm Exp $

C_MODULES?=	strlcpy strlcat getcwd memcpy fread memcpy declare \
		sscanf vararray md builtins
CPP_MODULES?=	snprintf sscanf

C_STRLCPY=	1 2 3 4 5 6
C_STRLCAT=	1 2 3 4 5 6
C_GETCWD=	1 2 3 4
C_MEMCPY=	1 2 3 4 5 6
C_FREAD=	1 2 3 4
C_DECLARE=	1 2 3 4 5 6 7 8 9 10 11 12 13
C_SSCANF=	1 2 3 4 5
C_VARARRAY=	1 2
C_MD=		1 2 3 4 5 6
C_BUILTINS=	1

CPP_SNPRINTF=	1 2 3 4 5
CPP_SSCANF=	1

TCC=		gcc
TCFLAGS=	-Wbounded -Wformat -pedantic
TCXX=		g++
TCXXFLAGS=	${TCFLAGS}
REGRESS_TARGETS= ${C_TARGETS} ${CPP_TARGETS}

.for i in ${C_MODULES}
.  for j in ${C_${i:U}}
C_TARGETS+= c-${i}-${j} 
c-${i}-${j}:
	${TCC} ${TCFLAGS} -o /dev/null ${i}-${j}.c 2>&1 | diff -u - ${.CURDIR}/${i}-${j}.c${EXP}
.  endfor
.endfor

.for i in ${CPP_MODULES}
.  for j in ${CPP_${i:U}}
CPP_TARGETS+= cpp-${i}-${j}
cpp-${i}-${j}:
	${TCXX} ${TCXXFLAGS} -o /dev/null ${i}-${j}.cpp 2>&1 | grep -v 'misused' | diff -u - ${.CURDIR}/${i}-${j}.cpp${EXP}
.  endfor
.endfor

generate:
.for i in ${C_TARGETS}
	-${TCC} ${TCFLAGS} -o /dev/null ${i:C/^c-//g}.c >${i:C/^c-//g}.c${EXP} 2>&1
.endfor
.for i in ${CPP_TARGETS}
	-${TCXX} ${TCXXFLAGS} -o /dev/null ${i:C/^cpp-//g}.cpp >${i:C/^cpp-//g}.cpp${EXP} 2>&1
.endfor

.PHONY:	${REGRESS_TARGETS} generate regress

.include <bsd.regress.mk>
.if defined(USE_GCC3)
EXP=		.exp.gcc3
.else
EXP=		.exp
.endif
