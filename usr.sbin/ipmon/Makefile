#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:43:46 deraadt Exp $

PROG=	ipmon
MAN=	ipmon.8	
CFLAGS+=-DIPL_NAME=\"/dev/ipl\" -DLOGFAC=LOG_LOCAL0 \
	-I${.CURDIR}/../../sys/netinet


.include <bsd.prog.mk>
