#	$OpenBSD: Makefile,v 1.6 1998/07/24 00:11:00 millert Exp $

PROG=	talk
DPADD=	${LIBCURSES}
LDADD=	-lcurses
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
