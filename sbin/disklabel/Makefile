#	$OpenBSD: Makefile,v 1.17 1998/04/25 07:04:45 deraadt Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.8 disklabel.5

CLEANFILES+=manual.c
manual.c:	disklabel.cat8
	(echo 'char manpage[] = "\\'; \
	nroff -mandoc ${.CURDIR}/disklabel.8 | sed -e 's/$$/\\n\\/' ; \
	echo '";' ) > manual.c

.if (${MACHINE} == "amiga")
CFLAGS+= -D${MACHINE}
.endif

.if (${MACHINE} == "hp300") || (${MACHINE} == "pmax")
CFLAGS+= -DOLD_SCSI
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
