#	$OpenBSD: Makefile,v 1.3 1996/06/23 14:30:52 deraadt Exp $

PROG=	ipf
MAN=	ipf.1 ipf.4 ipf.5
SRCS=	ipf.c parse.c opt.c
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I${.CURDIR}/../../sys/netinet


.include <bsd.prog.mk>
