#	$OpenBSD: Makefile,v 1.3 1996/04/19 16:08:38 niklas Exp $
#	$NetBSD: Makefile,v 1.1 1995/06/18 01:07:04 cgd Exp $

AWK=	awk --posix

pcidevs.h pcidevs_data.h: pcidevs devlist2h.awk
	/bin/rm -f pcidevs.h pcidevs_data.h
	${AWK} -f devlist2h.awk pcidevs
