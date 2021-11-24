#	$OpenBSD: Makefile,v 1.4 2021/11/24 12:58:01 visa Exp $

PROGS=		poll_close pollnval pollretval
LDADD=		-lpthread
WARNINGS=	yes

run-regress-poll_close:
	# Disabled until poll(2) uses kqueue as backend.
	@echo DISABLED

.include <bsd.regress.mk>
