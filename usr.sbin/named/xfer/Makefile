#	from: @(#)Makefile	5.1 (Berkeley) 5/28/90
#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:47:56 deraadt Exp $

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c
CFLAGS+=-I${.CURDIR}/..
.PATH:	${.CURDIR}/..
BINDIR=	/usr/libexec
MAN=	named-xfer.8

.include "../../Makefile.inc"
.include <bsd.prog.mk>
