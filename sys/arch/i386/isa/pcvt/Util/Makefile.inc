#	$OpenBSD: Makefile.inc,v 1.8 1999/01/13 07:26:03 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.3 1995/10/07 21:51:42 jtc Exp $

ECHO	?=	echo

BINDIR	=	/usr/sbin
FONTDIR	=	/usr/share/misc/pcvtfonts
LIBDIR	=	/usr/lib
LIBMODE	=	444

MAN3EXT	=	3
MAN4EXT	=	4
MAN5EXT	=	5
MAN8EXT	=	8

MAN = ${MAN1} ${MAN3} ${MAN4} ${MAN5} ${MAN8}

.if	empty(MAN) && defined(PROG)
MAN+=	${PROG}.1
.endif
