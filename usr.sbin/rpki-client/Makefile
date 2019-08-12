#	$OpenBSD: Makefile,v 1.7 2019/08/12 18:03:17 jsing Exp $

PROG=	rpki-client
SRCS=	as.c cert.c cms.c crl.c io.c ip.c log.c main.c mft.c \
	output-bgpd.c roa.c rsync.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD=	/usr/local/lib/eopenssl/libcrypto.a
CFLAGS= -I/usr/local/include/eopenssl

.include <bsd.prog.mk>
