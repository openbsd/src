#	$Id: Makefile,v 1.2 1996/02/15 04:18:02 etheisen Exp $

PROG=	pppd
SRCS=	main.c magic.c fsm.c lcp.c ipcp.c upap.c chap.c md5.c ccp.c \
	auth.c options.c sys-bsd.c
MAN=	pppd.8
SUBDIR=	pppstats chat
BINMODE=4555
BINOWN=	root

#LDADD=	-lcrypt -lutil
LDADD=	-lutil
#DPADD=	${LIBCRYPT} ${LIBUTIL}
DPADD=	${LIBUTIL}
CFLAGS+= -DHAVE_PATHS_H

.include <bsd.prog.mk>
