#	$NetBSD: Makefile,v 1.1.1.1 1995/07/25 23:12:28 chuck Exp $

#
# sboot needs GNU ld because it can generate S-Records
#

COMPILE.s= $(AS) $(ASFLAGS) -o $*.o
GNU_LD=/usr/local/bin/ld
LDFLAGS=-oformat srec -x -N -Ttext 0x4000 -e start
NOMAN=
PROG=	sboot
SRCS=	start.s clock.c console.c etherfun.c le_poll.c libc_sa.c \
		oc_cksum.s sboot.c

${PROG}: ${OBJS}
	${GNU_LD} ${LDFLAGS} -o ${.TARGET} ${OBJS}

.include <bsd.prog.mk>

