#	$OpenBSD: Makefile,v 1.1 2014/05/22 19:39:37 miod Exp $

AWK=	awk

PROG=	makemap.awk
MAP=	../../../dev/pckbc/wskbdmap_mfii.c

all:	wskbdmap_sgi.c

wskbdmap_sgi.c: ${MAP} ${PROG}
	/bin/rm -f $@
	${AWK} -f ${PROG} ${MAP} > $@
