#	$OpenBSD: Makefile,v 1.4 2000/04/14 02:52:35 itojun Exp $

PROG=	rarpd
SRCS=	rarpd.c arptab.c
MAN=	rarpd.8
CPPFLAGS+=-DHAVE_IFADDRS_H

# Uncomment the following to require a boot file in TFTP_DIR
# to exist for a rarp reply to be sent.
# CFLAGS+=-DREQUIRE_TFTPBOOT

.include <bsd.prog.mk>
