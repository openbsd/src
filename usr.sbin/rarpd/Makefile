#	$OpenBSD: Makefile,v 1.6 2003/05/15 14:52:12 itojun Exp $

PROG=	rarpd
SRCS=	rarpd.c arptab.c
MAN=	rarpd.8
LDADD+=	-lutil
DPADD+=	${LIBUTIL}

# Uncomment the following to require a boot file in TFTP_DIR
# to exist for a rarp reply to be sent.
# CFLAGS+=-DREQUIRE_TFTPBOOT

.include <bsd.prog.mk>
