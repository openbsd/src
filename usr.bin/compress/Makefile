#	$OpenBSD: Makefile,v 1.15 2003/07/13 04:43:34 millert Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c
MAN=	compress.1 zmore.1 zdiff.1
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
	    ${.CURDIR}/zmore ${.CURDIR}/zdiff ${DESTDIR}${BINDIR}
#	    ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
#	    ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
