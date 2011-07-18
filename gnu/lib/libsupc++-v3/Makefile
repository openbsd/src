# $OpenBSD: Makefile,v 1.3 2011/07/18 18:32:47 matthieu Exp $

.include "${.CURDIR}/../libstdc++-v3/Makefile.common" 

# Static only.
LIB=	supc++

# Pulled from Makefile.common
SRCS=	${SUPSRCS}

.if ${MACHINE_ARCH} == "arm"
CFLAGS+=	-D_GLIBCXX_SJLJ_EXCEPTIONS=1
.endif

# Build a standard .a lib with PIC objects
NOPIC=
CFLAGS+= ${PICFLAG}

.include <bsd.lib.mk>
