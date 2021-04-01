#	$OpenBSD: Makefile,v 1.20 2021/04/01 06:43:23 claudio Exp $

PROG=	rpki-client
SRCS=	as.c cert.c cms.c crl.c encoding.c gbr.c http.c io.c ip.c log.c \
	main.c mft.c mkdir.c output.c output-bgpd.c output-bird.c \
	output-csv.c output-json.c parser.c roa.c rsync.c tal.c validate.c \
	x509.c
MAN=	rpki-client.8

LDADD+= -ltls -lssl -lcrypto -lutil
DPADD+= ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL}

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith
#CFLAGS+= -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
