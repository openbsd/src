#	$OpenBSD: Makefile,v 1.9 1997/07/07 02:57:04 deraadt Exp $

PROG=	compress
SRCS=	main.c zopen.c gzopen.c
MAN=	compress.1
LINKS=	${BINDIR}/compress ${BINDIR}/uncompress \
	${BINDIR}/compress ${BINDIR}/zcat \
#	${BINDIR}/compress ${BINDIR}/gzip \
#	${BINDIR}/compress ${BINDIR}/gunzip \
#	${BINDIR}/compress ${BINDIR}/gzcat
MLINKS= compress.1 uncompress.1 \
	compress.1 zcat.1 \
#	compress.1 gzip.1 \
#	compress.1 gunzip.1 \
#	compress.1 gzcat.1

LDADD=-lz
DPADD=${LIBZ}

.include <bsd.prog.mk>
