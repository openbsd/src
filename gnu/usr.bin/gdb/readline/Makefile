# $Id: Makefile,v 1.2 1996/07/17 05:42:26 michaels Exp $

LIB=		readline
NOPROFILE=
NOPIC=

CFLAGS+=	-DVOID_SIGHANDLER -DVI_MODE -DHAVE_UNISTD_H \
		-I$(.CURDIR)/../include/
SRCS=		funmap.c history.c keymaps.c readline.c vi_mode.c tilde.c

install:

.include <bsd.lib.mk>
