#	$OpenBSD: Makefile,v 1.53 2011/03/20 23:36:42 schwarze Exp $

.include <bsd.own.mk>

VERSION=1.10.9
CFLAGS+=-DVERSION=\"${VERSION}\"
CFLAGS+=-W -Wall -Wstrict-prototypes

.if ${COMPILER_VERSION:L} == "gcc3" || ${COMPILER_VERSION:L} == "gcc4" 
CFLAGS+=-Wno-unused-parameter
.endif

SRCS=	roff.c tbl.c tbl_opts.c tbl_layout.c tbl_data.c eqn.c mandoc.c
SRCS+=	mdoc_macro.c mdoc.c mdoc_hash.c mdoc_strings.c \
	mdoc_argv.c mdoc_validate.c lib.c att.c \
	arch.c vol.c msec.c st.c
SRCS+=	man_macro.c man.c man_hash.c man_validate.c man_argv.c
SRCS+=	main.c mdoc_term.c chars.c term.c tree.c man_term.c
SRCS+=	html.c mdoc_html.c man_html.c out.c
SRCS+=	term_ps.c term_ascii.c tbl_term.c tbl_html.c

PROG=	mandoc

.include <bsd.prog.mk>
