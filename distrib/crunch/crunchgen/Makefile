#	$OpenBSD: Makefile,v 1.2 1996/09/25 06:40:52 etheisen Exp $

PROG=		crunchgen
SRCS=		crunchgen.c crunched_skel.c
CFLAGS+=	-g -Wall
CPPFLAGS+=	-DMF_NAMES='"Makefile.bsd-wrapper","Makefile"'
CLEANFILES+=	crunched_skel.c

crunched_skel.c: crunched_main.c
	sh ${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c > crunched_skel.c

.include <bsd.prog.mk>
