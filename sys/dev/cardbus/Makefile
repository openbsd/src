#	$OpenBSD: Makefile,v 1.1 2001/01/27 21:16:00 deraadt Exp $
#	$NetBSD: Makefile.cardbusdevs,v 1.2 1999/10/15 06:42:21 haya Exp $

AWK=	awk

cardbusdevs.h cardbusdevs_data.h: cardbusdevs devlist2h.awk
	/bin/rm -f cardbusdevs.h cardbusdevs_data.h
	${AWK} -f devlist2h.awk cardbusdevs
