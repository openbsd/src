#	$OpenBSD: Makefile,v 1.11 1999/02/24 23:33:11 deraadt Exp $
#	$NetBSD: Makefile,v 1.17 1995/10/24 22:18:34 jtc Exp $

PROG=	gzip
SRCS=	gzip.c zip.c deflate.c trees.c bits.c unzip.c inflate.c util.c \
	crypt.c lzw.c unlzw.c unlzh.c unpack.c getopt.c
MAN=	gzexe.1 gzip.1 zdiff.1 zforce.1 zgrep.1 zmore.1 znew.1
CFLAGS+=-DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DDIRENT=1

.if (${MACHINE_ARCH} == "m68k" || ${MACHINE_ARCH} == "i386")
SRCS+=	match.S
CFLAGS+=-DASMV 
.else
CFLAGS+=-DDYN_ALLOC
.endif

CLEANFILES+=	gzip.info

LDSTATIC= ${STATIC}

MAKEINFO=	makeinfo

MLINKS+= gzip.1 gunzip.1 gzip.1 gzcat.1 gzip.1
MLINKS+= zdiff.1 zcmp.1
#MLINKS+= gzip.1 uncompress.1
LINKS+= ${BINDIR}/gzip ${BINDIR}/gunzip
LINKS+= ${BINDIR}/gzip ${BINDIR}/gzcat
LINKS+=	${BINDIR}/zdiff ${BINDIR}/zcmp
#LINKS+= ${BINDIR}/gzip ${BINDIR}/uncompress

all:	gzip.info

gzip.info:	gzip.texi
	${MAKEINFO} -I${.CURDIR} ${.CURDIR}/gzip.texi

beforeinstall:
	${INSTALL} ${INSTALL_COPY} -m 444 -o ${BINOWN} -g ${BINGRP} *.info* \
		${DESTDIR}/usr/share/info

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	   ${.CURDIR}/zmore ${.CURDIR}/zdiff ${.CURDIR}/zgrep \
	   ${DESTDIR}${BINDIR}
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zgrep ${DESTDIR}${BINDIR}/zegrep
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	   ${.CURDIR}/zgrep ${DESTDIR}${BINDIR}/zfgrep

.if (${MACHINE_ARCH} == "m68k" || ${MACHINE_ARCH} == "i386")
match.o: ${.CURDIR}/match.S
	$(CPP) -D__OpenBSD__ -D${MACHINE_ARCH} ${.CURDIR}/match.S >_match.s
	$(CC) -c _match.s
	mv _match.o match.o
	rm -f _match.s
.endif

.include <bsd.prog.mk>
