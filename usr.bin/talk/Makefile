#	$OpenBSD: Makefile,v 1.4 1997/09/21 11:51:06 deraadt Exp $

PROG=	talk
DPADD=	${LIBCURSES} ${LIBTERMLIB} ${LIBCOMPAT}
LDADD=	-lcurses -ltermlib -lcompat
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
