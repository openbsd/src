#	$OpenBSD: Makefile,v 1.6 2000/05/27 05:52:12 deraadt Exp $

.if ${MACHINE} == "i386"
PROG=		kcon
DEVICE=		/dev/ttyC0
CFLAGS+= 	-I${.CURDIR}/../keycap -DKEYB_DEVICE=\"${DEVICE}\"

# the -Lfoo could be omitted if libkeycap.a were installed before
# making those programs here

LDADD =       -L${.CURDIR}/../keycap -L${.CURDIR}/../keycap/obj -lkeycap
.else
NOPROG=yes
.endif

MAN1=	kcon.1
MANSUBDIR=/i386

.include <bsd.prog.mk>
