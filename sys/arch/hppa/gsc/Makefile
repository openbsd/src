#	$OpenBSD: Makefile,v 1.1 2003/02/16 01:42:49 miod Exp $

AWK=	awk
PROG=	makemap.awk

SOURCE=	../../../dev/pckbc/wskbdmap_mfii.c

gsckbdmap.c: ${SOURCE} ${PROG}
	/bin/rm -f $@
	${AWK} -f ${PROG} ${SOURCE} > $@
