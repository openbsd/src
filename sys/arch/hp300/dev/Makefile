#	$OpenBSD: Makefile,v 1.1 1997/01/12 15:12:17 downsj Exp $
#	$NetBSD: Makefile,v 1.1 1996/12/17 08:40:59 thorpej Exp $

AWK=	awk

diodevs.h diodevs_data.h: diodevs devlist2h.awk
	/bin/rm -f diodevs.h diodevs_data.h
	${AWK} -f devlist2h.awk diodevs
