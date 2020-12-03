#	$OpenBSD: Makefile,v 1.3 2020/12/03 19:16:57 anton Exp $

PROG=		vnode

SRCS+=		vnode.c
SRCS+=		test-deadlock.c

WARNINGS=	yes

TESTS+=	deadlock

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

.for t in ${TESTS}
${t}:
	@echo "\n======== ${@} ========"
	f=`mktemp`; trap 'rm $$f' EXIT; ./${PROG} -f $$f ${t}
REGRESS_TARGETS+=	${t}
.endfor

.include <bsd.regress.mk>
