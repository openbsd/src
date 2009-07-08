#	$OpenBSD: Makefile,v 1.7 2009/07/08 00:04:10 schwarze Exp $

.include <bsd.own.mk>

VERSION=1.7.22
CFLAGS+=-DVERSION=\"${VERSION}\"
CFLAGS+=-W -Wall -Wstrict-prototypes
.if ${USE_GCC3:L} != "no"
CFLAGS+=-Wno-unused-parameter
.endif

SRCS=	mandoc.c mdoc_macro.c mdoc.c mdoc_hash.c mdoc_strings.c \
	mdoc_argv.c mdoc_validate.c mdoc_action.c lib.c att.c \
	arch.c vol.c msec.c st.c
SRCS+=	man_macro.c man.c man_hash.c man_validate.c man_action.c
SRCS+=	main.c mdoc_term.c ascii.c term.c tree.c man_term.c

PROG=	  mandoc

.include <bsd.prog.mk>
