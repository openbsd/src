#	$OpenBSD: Makefile,v 1.2 2002/02/11 19:16:26 deraadt Exp $

.if (${MACHINE_ARCH} == "vax")
CFLAGS+=-O1
.endif

PROG=	scsi
MAN=	scsi.8
LDADD=	-lutil
DPADD=	${LIBUTIL}

.include <bsd.prog.mk>
