#	$OpenBSD: Makefile,v 1.3 1996/06/26 05:40:19 deraadt Exp $
#	$NetBSD: Makefile,v 1.3 1994/12/09 02:14:09 jtc Exp $

PROG=	talk
DPADD=	${LIBCURSES} ${LIBTERMLIB} ${LIBCOMPAT}
LDADD=	-lcurses -ltermlib -lcompat
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
