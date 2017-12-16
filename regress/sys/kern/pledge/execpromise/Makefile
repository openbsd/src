#	$OpenBSD: Makefile,v 1.1 2017/12/16 11:00:49 florian Exp $

PROG=	execpromise

test_normal: ${PROG}
	@echo '\n======== $@ ========'
	${.OBJDIR}/${PROG} "stdio rpath inet" "stdio inet"
REGRESS_TARGETS+=	test_normal

test_no_child_pledge: ${PROG}
	@echo '\n======== $@ ========'
	${.OBJDIR}/${PROG} "stdio rpath inet"
REGRESS_TARGETS+=	test_no_child_pledge

test_abort_child: ${PROG}
	@echo '\n======== $@ ========'
	-${.OBJDIR}/${PROG} "stdio rpath"; \
	if [[ $$? == 134 ]] ; then echo OK; else false; fi
REGRESS_TARGETS+=	test_abort_child

test_ENOSYS_child: ${PROG}
	@echo '\n======== $@ ========'
	-${.OBJDIR}/${PROG} "stdio rpath error"; \
	if [[ $$? == 23 ]] ; then echo OK; else false; fi
REGRESS_TARGETS+=	test_ENOSYS_child

test_upgrade_fail: ${PROG}
	@echo '\n======== $@ ========'
	-${.OBJDIR}/${PROG} "stdio rpath inet" "stdio inet wpath"; \
	if [[ $$? == 24 ]] ; then echo OK; else false; fi
REGRESS_TARGETS+=	test_upgrade_fail

test_upgrade_ignore: ${PROG}
	@echo '\n======== $@ ========'
	${.OBJDIR}/${PROG} "stdio rpath inet error" "stdio rpath wpath inet"
REGRESS_TARGETS+=	test_upgrade_ignore


.include <bsd.regress.mk>
