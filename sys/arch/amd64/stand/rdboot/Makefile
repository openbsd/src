#	$OpenBSD: Makefile,v 1.1 2025/11/12 11:34:36 hshoexer Exp $

NOMAN=

.if ${MACHINE} == "amd64"
PROG=		rdboot
SRCS=		cmd.c disk.c rdboot.c vars.c
LDADD+=		-lutil -lz
LDSTATIC+=	-static
.else
NOPROG=
.endif

.PATH:  ${S}/lib/libsa
#SRCS+=  hexdump.c strtoll.c

install:

.include <bsd.prog.mk>
