#	$OpenBSD: Makefile,v 1.10 1999/11/17 05:02:23 millert Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c
MAN=	compress.1
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress \
	${BINDIR}/compress ${BINDIR}/zcat \
#	${BINDIR}/compress ${BINDIR}/gzip \
#	${BINDIR}/compress ${BINDIR}/gunzip \
#	${BINDIR}/compress ${BINDIR}/gzcat
MLINKS= zopen.3 zdopen.3 \
	compress.1 uncompress.1 \
	compress.1 zcat.1 \
#	compress.1 gzip.1 \
#	compress.1 gunzip.1 \
#	compress.1 gzcat.1

LDADD=-lz
DPADD=${LIBZ}

.include <bsd.prog.mk>
