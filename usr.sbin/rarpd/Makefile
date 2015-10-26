#	$OpenBSD: Makefile,v 1.8 2015/10/26 10:34:18 tedu Exp $

PROG=	rarpd
SRCS=	rarpd.c arptab.c
MAN=	rarpd.8

# Uncomment the following to require a boot file in TFTP_DIR
# to exist for a rarp reply to be sent.
# CFLAGS+=-DREQUIRE_TFTPBOOT

.include <bsd.prog.mk>
