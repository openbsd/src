#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:30:59 deraadt Exp $

PROG=	ipnat
MAN=	ipnat.1 ipnat.4 ipnat.5
SRCS=	ipnat.c kmem.c 
.PATH:  ${.CURDIR}/../ipfstat
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -I${.CURDIR}/../../sys/netinet

.include <bsd.prog.mk>
