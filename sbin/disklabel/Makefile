#	$OpenBSD: Makefile,v 1.68 2020/07/21 13:56:08 deraadt Exp $

PROG=	disklabel
SRCS=	disklabel.c dkcksum.c editor.c manual.c
DPADD=	${LIBUTIL}
LDADD=	-lutil
MAN=	disklabel.8 disklabel.5

CLEANFILES += disklabel.cat8 manual.c

.include <bsd.own.mk>

.ifdef NOMAN
manual.c:
	(echo 'const unsigned char manpage[] = {'; \
	echo 'no manual' | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.else
disklabel.cat8:	disklabel.8
	mandoc -Tascii ${.ALLSRC} > ${.TARGET}

manual.c:	disklabel.cat8
	(echo 'const unsigned char manpage[] = {'; \
	cat disklabel.cat8 | gzip -9c | hexdump -ve '"0x" 1/1 "%02x,"'; \
	echo '};'; echo 'const int manpage_sz = sizeof(manpage);') > manual.c
.endif

.if ${MACHINE} == "alpha"
CFLAGS+= -DSEEALSO="\"installboot(8)\""
.endif

.if ${MACHINE} == "amd64" || ${MACHINE} == "i386" || \
    ${MACHINE} == "octeon" || ${MACHINE} == "loongson" || \
    ${MACHINE} == "armv7" || ${MACHINE} == "arm64" || \
    ${MACHINE} == "landisk" || ${MACHINE} == "powerpc64"
CFLAGS+= -DSEEALSO="\"fdisk(8), installboot(8)\""
.endif

.if ${MACHINE} == "macppc"
CFLAGS+= -DSEEALSO="\"fdisk(8), pdisk(8), installboot(8)\""
.endif

.if ${MACHINE} == "sparc64"
CFLAGS+= -DSEEALSO="\"installboot(8)\"" -DSUN_CYLCHECK -DSUN_AAT0
.endif

.include <bsd.prog.mk>
