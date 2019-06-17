#	$OpenBSD: Makefile,v 1.3 2019/06/17 15:11:12 deraadt Exp $

PROG=	rpki-client
SRCS=	cert.c cms.c crl.c io.c ip.c log.c mft.c \
	output-bgpd.c roa.c rsync.c tal.c validate.c x509.c
MAN=	rpk-client.8

LDADD=	/usr/local/lib/eopenssl/libssl.a /usr/local/lib/eopenssl/libcrypto.a
CFLAGS= -I/usr/local/include/eopenssl -I${.OBJDIR}

.include <bsd.prog.mk>
