#	$OpenBSD: Makefile,v 1.37 2005/02/16 20:39:37 millert Exp $

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
	cat disklabel.cat8 | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.endif

.if (${MACHINE} == "alpha")
CFLAGS+= -DSEEALSO="\"fdisk(8), installboot(8)\""
.endif

.if (${MACHINE} == "cats") || (${MACHINE} == "zaurus")
CFLAGS+= -DSEEALSO="\"fdisk(8)\""
.endif

.if (${MACHINE} == "i386") || (${MACHINE} == "amd64")
CFLAGS+= -DNUMBOOT=2 -DDOSLABEL
CFLAGS+= -DSEEALSO="\"fdisk(8), installboot(8)\""
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
