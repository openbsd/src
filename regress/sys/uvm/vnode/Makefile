PROG=		vnode
WARNINGS=	yes

REGRESS_SETUP_ONCE=	setup
setup: ${PROG}

REGRESS_TARGETS+=	run-vnode
run-vnode:
	t=`mktemp`; trap 'rm $$t' EXIT; ${.OBJDIR}/vnode $$t

.include <bsd.regress.mk>
