#       $OpenBSD: Makefile,v 1.3 2003/06/19 14:32:36 markus Exp $

PROG=	eventtest
LDADD=	-levent
DPADD=	${LIBEVENT}

REGRESS_TARGETS=	eventtest-select eventtest-kqueue
.PHONY: ${REGRESS_TARGETS}

eventtest-kqueue: ${PROG}
	@EVENT_SHOW_METHOD=yes EVENT_NOSELECT=yes ./${PROG} 

eventtest-select: ${PROG}
	@EVENT_SHOW_METHOD=yes EVENT_NOKQUEUE=yes ./${PROG} 

.include <bsd.regress.mk>
