#	$OpenBSD: Makefile,v 1.2 2020/11/05 18:25:39 anton Exp $

PROG=		vnode
WARNINGS=	yes

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

REGRESS_TARGETS+=	run-vnode
run-vnode:
	t=`mktemp`; trap 'rm $$t' EXIT; ${.OBJDIR}/vnode $$t

.include <bsd.regress.mk>
