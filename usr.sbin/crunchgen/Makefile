#	$OpenBSD: Makefile,v 1.4 2009/12/04 04:59:48 drahn Exp $

PROG=		crunchgen
MAN=		crunchgen.8
SRCS=		crunchgen.c crunched_skel.c \
		crunchide.c elf_hide.c ecoff_hide.c mangle.c
CFLAGS+=	-g -Wall
CLEANFILES+=	crunched_skel.c

crunched_skel.c: crunched_main.c
	sh ${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c > crunched_skel.c

.include <bsd.prog.mk>
