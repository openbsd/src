#	$OpenBSD: Makefile,v 1.10 2019/11/02 13:38:33 jsing Exp $

PROG=	rpki-client
SRCS=	as.c cert.c cms.c crl.c io.c ip.c log.c main.c mft.c \
	output-bgpd.c output-bird.c output-csv.c output-json.c \
	roa.c rsync.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}

.include <bsd.prog.mk>
