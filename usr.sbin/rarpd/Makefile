#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:10 deraadt Exp $

PROG=	rarpd
SRCS=	rarpd.c arptab.c
MAN=	rarpd.8

# Uncomment the following to require a boot file in TFTP_DIR
# to exist for a rarp reply to be sent.
# CFLAGS+=-DREQUIRE_TFTPBOOT

.include <bsd.prog.mk>
