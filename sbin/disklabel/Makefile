#	$OpenBSD: Makefile,v 1.26 2001/09/01 16:21:16 drahn Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.8 disklabel.5


CLEANFILES+=manual.c

.ifdef NOMAN
manual.c:
	(echo 'char manpage[] = "\\'; \
	echo "no manual"; \
	echo '";' ) > manual.c
.else
manual.c:	disklabel.cat8
	(echo 'char manpage[] = "\\'; \
	sed -e 's/[\\"]/\\&/g' -e 's/$$/\\n\\/' disklabel.cat8; \
	echo '";' ) > manual.c
.endif

.if (${MACHINE} == "amiga")
CFLAGS+= -D${MACHINE}
.endif

.if (${MACHINE} == "i386")
CFLAGS+= -DNUMBOOT=2 -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8), installboot(8)\""
.endif

.if (${MACHINE} == "arc")
CFLAGS+= -DDOSLABEL -DNUMBOOT=1
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.if (${MACHINE} == "hp300") || (${MACHINE} == "vax")
CFLAGS+= -DNUMBOOT=1
.endif

.if (${MACHINE} == "sun3") || (${MACHINE} == "sparc")
CFLAGS+= -DSEEALSO="\"installboot(8)\"" -DCYLCHECK -DAAT0
.endif

.if (${MACHINE} == "mvme68k") || (${MACHINE} == "mvme68k")
CFLAGS+= -DSEEALSO="\"installboot(8)\""
.endif

.if (${MACHINE} == "macppc")
CFLAGS+= -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.include <bsd.prog.mk>
