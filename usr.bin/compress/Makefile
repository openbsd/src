#	$OpenBSD: Makefile,v 1.19 2003/09/05 04:46:35 tedu Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c nullopen.c
MAN=	compress.1 zmore.1 zdiff.1 zforce.1 gzexe.1 znew.1
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress \
	${BINDIR}/compress ${BINDIR}/zcat \
	${BINDIR}/compress ${BINDIR}/gzip \
	${BINDIR}/compress ${BINDIR}/gunzip \
	${BINDIR}/compress ${BINDIR}/gzcat \
	${BINDIR}/zdiff ${BINDIR}/zcmp

MLINKS= compress.1 uncompress.1 \
	compress.1 zcat.1 \
	compress.1 gzip.1 \
	compress.1 gunzip.1 \
	compress.1 gzcat.1 \
	zdiff.1 zcmp.1

LDADD=-lz
DPADD=${LIBZ}

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/zmore ${.CURDIR}/zdiff \
	    ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	    ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
