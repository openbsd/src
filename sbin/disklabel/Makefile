#	$OpenBSD: Makefile,v 1.11 1997/09/30 17:54:16 millert Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.5 disklabel.8

.if ${MACHINE} == "amiga"
CFLAGS+= -D${MACHINE}
.endif

.if (${MACHINE} == "i386")
CFLAGS+= -DNUMBOOT=2 -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8), installboot(8)\""
.endif

.if (${MACHINE} == "arc")
CFLAGS+= -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.if (${MACHINE} == "hp300") || (${MACHINE} == "vax")
CFLAGS+= -DNUMBOOT=1
.endif

.if (${MACHINE} == "sun3") || (${MACHINE} == "sparc")
CFLAGS+= -DSEEALSO="\"installboot(8)\"" -DCYLCHECK
.endif

.if (${MACHINE} == "mvme68k") || (${MACHINE} == "mvme68k")
CFLAGS+= -DSEEALSO="\"installboot(8)\""
.endif

.include <bsd.prog.mk>
