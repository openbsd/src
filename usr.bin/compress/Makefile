#	$OpenBSD: Makefile,v 1.16 2003/07/29 11:54:33 otto Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c
MAN=	compress.1 zmore.1 zdiff.1 zforce.1
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
	    ${.CURDIR}/zforce \
	    ${DESTDIR}${BINDIR}
#	    ${.CURDIR}/gzexe ${.CURDIR}/znew \

.include <bsd.prog.mk>
