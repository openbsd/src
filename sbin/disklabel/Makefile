#	$OpenBSD: Makefile,v 1.6 1996/09/20 08:25:01 deraadt Exp $
#	$NetBSD: Makefile,v 1.18 1995/10/17 16:34:04 cgd Exp $
#	@(#)Makefile	8.2 (Berkeley) 3/17/94

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.5 disklabel.8

.if ${MACHINE} == "amiga"
CFLAGS+= -D${MACHINE}
.endif

.if (${MACHINE} == "i386")
CFLAGS+= -DNUMBOOT=2 -DDOSLABEL
.endif

.if (${MACHINE} == "arc")
CFLAGS+= -DDOSLABEL
.endif

.if (${MACHINE} == "hp300") || (${MACHINE} == "vax")
CFLAGS+= -DNUMBOOT=1
.endif

.include <bsd.prog.mk>
