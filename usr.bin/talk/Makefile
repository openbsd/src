#	$NetBSD: Makefile,v 1.3 1994/12/09 02:14:09 jtc Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	talk
DPADD=	${LIBCURSES} ${LIBTERMLIB} ${LIBCOMPAT}
LDADD=	-lcurses -ltermlib -lcompat
SRCS=	ctl.c ctl_transact.c display.c get_addrs.c get_names.c \
	init_disp.c invite.c io.c look_up.c msgs.c talk.c

.include <bsd.prog.mk>
