#	$OpenBSD: Makefile,v 1.4 2019/11/28 00:17:12 bluhm Exp $

NOMAN=

.if ${MACHINE} == "octeon"
PROG=		rdboot
SRCS=		cmd.c disk.c rdboot.c vars.c
LDADD+=		-lutil
LDSTATIC+=	-static
.else
NOPROG=
.endif

.PATH:  ${S}/lib/libsa
SRCS+=  hexdump.c strtoll.c

install:

.include <bsd.prog.mk>
