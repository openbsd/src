#	$OpenBSD: Makefile,v 1.21 2007/05/20 00:27:27 jsg Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c nullopen.c
MAN=	compress.1 gzexe.1 gzip.1 zdiff.1 zforce.1 zmore.1 znew.1
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress \
	${BINDIR}/compress ${BINDIR}/zcat \
	${BINDIR}/compress ${BINDIR}/gzip \
	${BINDIR}/compress ${BINDIR}/gunzip \
	${BINDIR}/compress ${BINDIR}/gzcat \
	${BINDIR}/zdiff ${BINDIR}/zcmp \
	${BINDIR}/zmore ${BINDIR}/zless

MLINKS+=compress.1 uncompress.1 compress.1 zcat.1
MLINKS+=gzip.1 gunzip.1 gzip.1 gzcat.1
MLINKS+=zdiff.1 zcmp.1
MLINKS+=zmore.1 zless.1

LDADD=-lz
DPADD=${LIBZ}

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/zmore ${.CURDIR}/zdiff \
	    ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	    ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
