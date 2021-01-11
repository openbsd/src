# $OpenBSD: Makefile,v 1.2 2021/01/11 22:58:16 bluhm Exp $

.if ! exists(/usr/local/lib/libnetsnmpagent.a)
regress:
	@echo Install net-snmp package to run this regress.
	@echo SKIPPED
.endif

PROG=		agentx
SRCS=		main.c log.c
CFLAGS=		-Wall
NOMAN=		yes
LDADD=		-lagentx -levent

REGRESS_TARGETS=	run-regress-agentx

run-regress-agentx: ${PROG}
	${SUDO} ./${PROG}

.include <bsd.regress.mk>
