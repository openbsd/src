#	$OpenBSD: Makefile.cardbusdevs,v 1.1 2000/03/02 23:03:26 aaron Exp $
#	$NetBSD: Makefile.cardbusdevs,v 1.2 1999/10/15 06:42:21 haya Exp $

AWK=	awk

cardbusdevs.h cardbusdevs_data.h: cardbusdevs devlist2h.awk
	/bin/rm -f cardbusdevs.h cardbusdevs_data.h
	${AWK} -f devlist2h.awk cardbusdevs
