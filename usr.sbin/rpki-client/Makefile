#	$OpenBSD: Makefile,v 1.4 2019/06/17 15:36:56 jsg Exp $

PROG=	rpki-client
SRCS=	cert.c cms.c crl.c io.c ip.c log.c mft.c \
	output-bgpd.c roa.c rsync.c tal.c validate.c x509.c
MAN=	rpki-client.8

LDADD=	/usr/local/lib/eopenssl/libssl.a /usr/local/lib/eopenssl/libcrypto.a
CFLAGS= -I/usr/local/include/eopenssl -I${.OBJDIR}

.include <bsd.prog.mk>
