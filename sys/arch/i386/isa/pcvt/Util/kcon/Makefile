#	$OpenBSD: Makefile,v 1.3 1999/01/13 07:26:05 niklas Exp $

PROG=		kcon
DEVICE=		/dev/ttyC0
CFLAGS+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

# the -Lfoo could be omitted if libkeycap.a were installed before
# making those programs here

LDADD =       -L${.CURDIR}/../keycap -L${.CURDIR}/../keycap/obj -lkeycap

.include <bsd.prog.mk>
