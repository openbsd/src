#	@(#)Makefile	8.1 (Berkeley) 6/6/93
#       $NetBSD: Makefile,v 1.3 1995/09/05 09:11:55 phil Exp $

PROG=	gcore
SRCS=	gcore.c

.if ${MACHINE} == pc532
SRCS += md-nop.c
.else
SRCS += md-${MACHINE}.c
.endif

LDADD=	-lkvm

.include <bsd.prog.mk>
