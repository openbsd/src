#	$OpenBSD: Makefile,v 1.5 2001/12/01 23:27:23 miod Exp $

PROG=	rarpd
SRCS=	rarpd.c arptab.c
MAN=	rarpd.8
CPPFLAGS+=-DHAVE_IFADDRS_H
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

# Uncomment the following to require a boot file in TFTP_DIR
# to exist for a rarp reply to be sent.
# CFLAGS+=-DREQUIRE_TFTPBOOT

.include <bsd.prog.mk>
