# $OpenBSD: Makefile,v 1.3 2000/03/01 22:10:13 todd Exp $
# $NetBSD: Makefile,v 1.4 1996/03/27 04:48:43 scottr Exp $
# Build a smaller dd (i.e. for boot media)

PROG=	dd
NOMAN=	

SRCDIR= ${.CURDIR}/../../../bin/dd

SRCS=	args.c conv.c dd.c misc.c position.c
# SRCS+= conv_tab.c (not used)

CFLAGS+= -DNO_CONV -I${SRCDIR}

all: ${PROG}

.include <bsd.prog.mk>

.PATH:  ${SRCDIR}
