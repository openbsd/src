#	@(#)Makefile	5.12 (Berkeley) 5/7/93
#
# optional flags are: MEASURE TESTING DEBUG

.include <bsd.own.mk>

.if (${YP} == "yes")
CFLAGS+=-DHAVENIS
.endif
PROG=	timed
SRCS=	acksend.c candidate.c correct.c master.c networkdelta.c readmsg.c \
	slave.c timed.c byteorder.c measure.c cksum.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	timed.8

.include "../../Makefile.inc"
.include <bsd.prog.mk>
