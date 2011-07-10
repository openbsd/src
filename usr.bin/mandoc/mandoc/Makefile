#	$OpenBSD: Makefile,v 1.1 2011/07/10 02:37:22 schwarze Exp $

PROG=		mandoc
BINDIR=		/usr/bin

CFLAGS+=	-DVERSION=\"1.11.3\"
LDFLAGS+=	-L../lib
LDADD+=		-lmandoc
DPADD+=		../lib/libmandoc.a

.PATH:		${.CURDIR}/..

SRCS=	main.c tree.c \
	term.c term_ascii.c term_ps.c \
	mdoc_term.c man_term.c tbl_term.c \
	html.c out.c mdoc_html.c man_html.c tbl_html.c

.include <bsd.prog.mk>
