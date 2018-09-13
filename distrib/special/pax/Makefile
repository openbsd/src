#	$OpenBSD: Makefile,v 1.2 2018/09/13 16:34:33 sthen Exp $

.PATH:  ${.CURDIR}/../../../bin/pax
CFLAGS+=-DNOCPIO -I${.CURDIR}/../../../bin/pax

PROG=   pax
SRCS=	ar_io.c ar_subs.c buf_subs.c file_subs.c ftree.c\
	gen_subs.c getoldopt.c options.c pat_rep.c pax.c sel_subs.c tables.c\
	tar.c tty_subs.c

.include <bsd.prog.mk>
