#	$OpenBSD: Makefile,v 1.33 2010/04/07 23:15:05 schwarze Exp $

.include <bsd.own.mk>

VERSION=1.9.23
CFLAGS+=-DVERSION=\"${VERSION}\"
CFLAGS+=-W -Wall -Wstrict-prototypes
.if ${USE_GCC3:L} != "no"
CFLAGS+=-Wno-unused-parameter
.endif

SRCS=	mandoc.c mdoc_macro.c mdoc.c mdoc_hash.c mdoc_strings.c \
	mdoc_argv.c mdoc_validate.c mdoc_action.c lib.c att.c \
	arch.c vol.c msec.c st.c
SRCS+=	man_macro.c man.c man_hash.c man_validate.c \
	man_action.c man_argv.c
SRCS+=	main.c mdoc_term.c chars.c term.c tree.c man_term.c
SRCS+=	html.c mdoc_html.c man_html.c out.c

PROG=	mandoc

.include <bsd.prog.mk>
