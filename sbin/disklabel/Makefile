#	$OpenBSD: Makefile,v 1.13 1997/10/16 10:40:09 deraadt Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.5 disklabel.8

manual.c:	${.OBJDIR}/disklabel.cat8
	(echo 'char manpage[] = "\\'; \
	sed -e 's/$$/\\n\\/' ${.OBJDIR}/disklabel.cat8; \
	echo '";' ) > manual.c

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

.if (${MACHINE} == "powerpc")
CFLAGS+= -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.include <bsd.prog.mk>
