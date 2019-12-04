#	$OpenBSD: Makefile,v 1.14 2019/12/04 12:40:17 deraadt Exp $

PROG=	rpki-client
SRCS=	as.c cert.c cms.c crl.c io.c ip.c log.c main.c mft.c output.c \
	output-bgpd.c output-bird.c output-csv.c output-json.c \
	roa.c rsync.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD+= -lcrypto
DPADD+= ${LIBCRYPTO}

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith
#CFLAGS+= -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
