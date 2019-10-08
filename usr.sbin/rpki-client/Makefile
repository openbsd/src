#	$OpenBSD: Makefile,v 1.8 2019/10/08 10:04:36 claudio Exp $

PROG=	rpki-client
SRCS=	as.c cert.c cms.c crl.c io.c ip.c log.c main.c mft.c \
	output-bgpd.c output-json.c roa.c rsync.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD=	/usr/local/lib/eopenssl/libcrypto.a
CFLAGS= -I/usr/local/include/eopenssl

.include <bsd.prog.mk>
