#	$OpenBSD: Makefile,v 1.76 2014/04/18 10:00:48 schwarze Exp $

.include <bsd.own.mk>

CFLAGS  += -DVERSION=\"1.13.0\"
CFLAGS  += -W -Wall -Wstrict-prototypes -Wno-unused-parameter
LDADD	+= -lsqlite3

SRCS=	mandoc.c mandoc_aux.c read.c \
	roff.c tbl.c tbl_opts.c tbl_layout.c tbl_data.c eqn.c
SRCS+=	mdoc_macro.c mdoc.c mdoc_hash.c \
	mdoc_argv.c mdoc_validate.c lib.c att.c \
	arch.c vol.c msec.c st.c
SRCS+=	man_macro.c man.c man_hash.c man_validate.c
SRCS+=	main.c mdoc_term.c chars.c term.c tree.c man_term.c eqn_term.c
SRCS+=	mdoc_man.c
SRCS+=	html.c mdoc_html.c man_html.c out.c eqn_html.c
SRCS+=	term_ps.c term_ascii.c tbl_term.c tbl_html.c
SRCS+=	manpath.c mandocdb.c mansearch_const.c mansearch.c apropos.c

PROG=	mandoc

LINKS =	${BINDIR}/mandoc ${BINDIR}/apropos \
	${BINDIR}/mandoc ${BINDIR}/whatis \
	${BINDIR}/mandoc /usr/sbin/makewhatis \
	${BINDIR}/mandoc /usr/libexec/makewhatis

MAN =	apropos.1 mandoc.1 whatis.1 makewhatis.8

.include <bsd.prog.mk>
