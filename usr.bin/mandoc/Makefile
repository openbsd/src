#	$OpenBSD: Makefile,v 1.1 2009/04/06 20:30:40 kristaps Exp $

CFLAGS+=-W -Wall -Wstrict-prototypes -Wno-unused-parameter

SRCS=	mdoc_macro.c mdoc.c mdoc_hash.c mdoc_strings.c \
	mdoc_argv.c mdoc_validate.c mdoc_action.c lib.c att.c \
	arch.c vol.c msec.c st.c
SRCS+=	man_macro.c man.c man_hash.c man_validate.c man_action.c
SRCS+=	main.c mdoc_term.c ascii.c term.c tree.c man_term.c

PROG=	  mandoc

.include <bsd.prog.mk>
