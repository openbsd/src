#	$OpenBSD: Makefile,v 1.7 2002/06/18 00:40:30 ericj Exp $
#
#	@(#)Makefile	5.12 (Berkeley) 5/7/93
#
# optional flags are: MEASURE TESTING DEBUG

PROG=	timed
SRCS=	acksend.c candidate.c correct.c master.c networkdelta.c readmsg.c \
	slave.c timed.c byteorder.c measure.c cksum.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	timed.8

.include "../../Makefile.inc"
.include <bsd.prog.mk>
