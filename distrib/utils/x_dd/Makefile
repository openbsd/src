# $NetBSD: Makefile,v 1.3 1995/10/13 18:35:26 gwr Exp $
# Build a smaller dd (i.e. for boot media)

PROG=	dd
NOMAN=	

SRCDIR= ${BSDSRCDIR}/bin/dd

SRCS=	args.c conv.c dd.c misc.c position.c
# SRCS+= conv_tab.c (not used)

CFLAGS+= -DNO_CONV -I${SRCDIR}

all: ${PROG}

.include <bsd.prog.mk>

.PATH:  ${SRCDIR}
