#	from: @(#)Makefile	5.12 (Berkeley) 1/16/91
#	$NetBSD: Makefile,v 1.11 1995/08/17 17:22:06 thorpej Exp $

PROG=	config.old
BINDIR=	/usr/sbin
CFLAGS+=-I. -I${.CURDIR}
SRCS=	config.c main.c lang.c mkioconf.c mkmakefile.c mkglue.c mkheaders.c \
	mkswapconf.c specfile.c
MAN=	config.old.8
LDADD+=	-ll
DPADD+=	${LIBL}
CLEANFILES+=y.tab.h lang.c config.c y.tab.c

.include <bsd.prog.mk>
