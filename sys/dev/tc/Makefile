#	$OpenBSD: Makefile,v 1.1 1996/04/18 23:48:19 niklas Exp $
#	$NetBSD: Makefile,v 1.1 1996/03/02 01:16:47 cgd Exp $

AWK=	awk

tcdevs.h tcdevs_data.h: tcdevs devlist2h.awk
	/bin/rm -f tcdevs.h tcdevs_data.h
	${AWK} -f devlist2h.awk tcdevs
