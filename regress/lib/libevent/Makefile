#       $OpenBSD: Makefile,v 1.4 2003/09/23 08:52:04 markus Exp $

PROG=	eventtest
LDADD=	-levent
DPADD=	${LIBEVENT}

REGRESS_TARGETS=	eventtest-select eventtest-kqueue eventtest-poll
.PHONY: ${REGRESS_TARGETS}

eventtest-kqueue: ${PROG}
	@EVENT_SHOW_METHOD=yes EVENT_NOPOLL=yes EVENT_NOSELECT=yes ./${PROG} 

eventtest-select: ${PROG}
	@EVENT_SHOW_METHOD=yes EVENT_NOPOLL=yes EVENT_NOKQUEUE=yes ./${PROG} 

eventtest-poll: ${PROG}
	@EVENT_SHOW_METHOD=yes EVENT_NOSELECT=yes EVENT_NOKQUEUE=yes ./${PROG} 

.include <bsd.regress.mk>
