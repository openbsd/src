#       $OpenBSD: Makefile,v 1.2 2003/06/16 14:14:29 markus Exp $

PROG=	eventtest
LDADD=	-levent
DPADD=	${LIBEVENT}

REGRESS_TARGETS=	eventtest-select eventtest-kqueue
.PHONY: ${REGRESS_TARGETS}

eventtest-kqueue:
	@EVENT_SHOW_METHOD=yes EVENT_NOSELECT=yes ./${PROG} 

eventtest-select:
	@EVENT_SHOW_METHOD=yes EVENT_NOKQUEUE=yes ./${PROG} 

.include <bsd.regress.mk>
