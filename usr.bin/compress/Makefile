#	$OpenBSD: Makefile,v 1.22 2016/03/30 06:38:45 jmc Exp $

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

LDADD=-lz
DPADD=${LIBZ}

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${.CURDIR}/zmore ${.CURDIR}/zdiff \
	    ${.CURDIR}/zforce ${.CURDIR}/gzexe ${.CURDIR}/znew \
	    ${DESTDIR}${BINDIR}

.include <bsd.prog.mk>
