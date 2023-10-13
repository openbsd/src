#	$OpenBSD: Makefile,v 1.33 2023/10/13 12:06:49 job Exp $

PROG=	rpki-client
SRCS=	as.c aspa.c cert.c cms.c constraints.c crl.c encoding.c filemode.c \
	gbr.c geofeed.c http.c io.c ip.c json.c main.c mft.c mkdir.c ometric.c \
	output.c output-bgpd.c output-bird.c output-csv.c output-json.c \
	output-ometric.c parser.c print.c repo.c rfc3779.c roa.c \
	rrdp.c rrdp_delta.c rrdp_notification.c rrdp_snapshot.c rrdp_util.c \
	rsc.c rsync.c tak.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD+= -lexpat -ltls -lssl -lcrypto -lutil -lz
DPADD+= ${LIBEXPAT} ${LIBTLS} ${LIBSSL} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

CFLAGS+= -Wall -I${.CURDIR}
CFLAGS+= -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+= -Wmissing-declarations
CFLAGS+= -Wshadow -Wpointer-arith
#CFLAGS+= -Wcast-qual
CFLAGS+= -Wsign-compare

.include <bsd.prog.mk>
