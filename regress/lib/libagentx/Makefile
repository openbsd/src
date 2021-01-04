# $OpenBSD: Makefile,v 1.1 2021/01/04 19:24:55 rob Exp $

REGRESS_TARGETS= run-regress-agentx

NETSNMP = $((grep _netsnmp /etc/group))

.if empty (NETSNMP)
regress:
	@echo NETSNMP package required.
	@echo SKIPPED
.endif

SRCS=		main.c log.c
PROG=		agentx
CFLAGS=		-Wall
NOMAN=		yes
LDADD=		-lagentx -levent

run-regress-agentx:
	${SUDO} ${.OBJDIR}/agentx

.include <bsd.regress.mk>
