#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:47:58 deraadt Exp $

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c ccp.c \
	auth.c options.c sys-bsd.c
MAN=	pppd.8
SUBDIR=	pppstats chat
BINMODE=4555
BINOWN=	root

LDADD=	-lcrypt -lutil
DPADD=	${LIBCRYPT} ${LIBUTIL}
CFLAGS+= -DHAVE_PATHS_H

.include <bsd.prog.mk>
