#	$OpenBSD: Makefile,v 1.34 2004/11/06 18:57:59 otto Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.8 disklabel.5


CLEANFILES+=manual.c

.include <bsd.own.mk>

.ifdef NOMAN
manual.c:
	(echo 'const char manpage[] = {'; \
	echo 'no manual' | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.else
manual.c:	disklabel.cat8
	(echo 'const char manpage[] = {'; \
	gzip -9c disklabel.cat8 | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.endif

.if (${MACHINE} == "amiga")
CFLAGS+= -D${MACHINE}
.endif

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
CFLAGS+= -DNUMBOOT=2 -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8), installboot(8)\""
.endif

.if (${MACHINE} == "arc")
CFLAGS+= -DDOSLABEL -DNUMBOOT=1
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.if (${MACHINE} == "hp300") || (${MACHINE} == "vax") || (${MACHINE} == "hppa")
CFLAGS+= -DNUMBOOT=1
.endif

.if (${MACHINE} == "sun3") || (${MACHINE} == "sparc") || \
    (${MACHINE} == "sparc64")
CFLAGS+= -DSEEALSO="\"installboot(8)\"" -DCYLCHECK -DAAT0
.endif

.if (${MACHINE} == "mvme68k") || (${MACHINE} == "mvme88k")
CFLAGS+= -DSEEALSO="\"installboot(8)\""
.endif

.if (${MACHINE} == "macppc")
CFLAGS+= -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.include <bsd.prog.mk>
