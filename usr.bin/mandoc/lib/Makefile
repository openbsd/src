#	$OpenBSD: Makefile,v 1.1 2011/07/10 02:37:21 schwarze Exp $

LIB=		mandoc
NOPIC=		yes
NOPROFILE=	yes

.PATH:		${.CURDIR}/..

SRCS=	mandoc.c chars.c read.c \
	roff.c tbl.c tbl_opts.c tbl_layout.c tbl_data.c eqn.c \
	arch.c att.c lib.c msec.c st.c vol.c \
	mdoc.c mdoc_argv.c mdoc_hash.c mdoc_macro.c mdoc_validate.c \
	man.c man_hash.c man_macro.c man_validate.c

install:
	@:

.include <bsd.lib.mk>
