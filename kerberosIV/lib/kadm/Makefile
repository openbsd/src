# $OpenBSD: Makefile,v 1.2 2001/01/09 02:41:17 angelos Exp $

LIB	= kadm

KRBDEPTH=../..
.include "../../Makefile.inc"

INCLUDE_FILES	= ${KRBSRC}/lib/kadm/kadm.h kadm_err.h

.PATH:	${KRBSRC}/lib/kadm

CFLAGS	+= -I.

SRCS=	kadm_cli_wrap.c \
	kadm_err.c \
	kadm_stream.c \
	kadm_supp.c \
	check_password.c

beforedepend prereq: kadm_err.h

includes:	kadm_err.h

kadm_err.c kadm_err.h:
	${COMPILE_ET} ${KRBSRC}/lib/kadm/kadm_err.et

CLEANFILES	+= kadm_err.c kadm_err.h

.include <bsd.lib.mk>
