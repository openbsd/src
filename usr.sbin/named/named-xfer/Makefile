#	$OpenBSD: Makefile,v 1.6 2002/11/14 23:23:43 millert Exp $
#	$NetBSD: Makefile,v 1.2 1996/02/04 12:02:12 mrg Exp $
# from: $Id: Makefile,v 8.2 1995/12/31 23:28:01 vixie Exp 

.PATH:	${.CURDIR}/../named \
	${.CURDIR}/../man

PROG=	named-xfer
SRCS=	named-xfer.c db_glue.c storage.c version.c
CFLAGS+= ${INCLUDE} -I${.CURDIR}/../named ${CONFIG}
MAN=	named-xfer.8
BINDIR=	/var/named
LDSTATIC= ${STATIC}

version.c: Version.c ${.CURDIR}/../Makefile
	(u=root d=`pwd` h=localhost t=`date +%c`; \
	sed -e "s|%WHEN%|$${t}|" -e "s|%VERSION%|"${VER}"|" \
	-e "s|%WHOANDWHERE%|$${u}@$${h}:$${d}|" \
	< ${.CURDIR}/../named/Version.c > version.c)

.include <bsd.prog.mk>
.include "../../Makefile.inc"
